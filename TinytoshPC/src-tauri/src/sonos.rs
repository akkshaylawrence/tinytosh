use std::collections::BTreeMap;
use std::net::Ipv4Addr;
use std::thread;
use std::time::Duration;
use any_ascii::any_ascii;
use mdns_sd::{ServiceDaemon, ServiceEvent};
use tauri::Manager;

use crate::{AppState, MediaStats};

const SONOS_SERVICE_TYPE: &str = "_sonos._tcp.local.";
const SONOS_PORT: u16 = 1400;
const SONOS_POLL_INTERVAL_MS: u64 = 2000;
const SONOS_REQUEST_TIMEOUT_MS: u64 = 500;

const DIDL_NOT_IMPLEMENTED: &str = "NOT_IMPLEMENTED";
const GROUP_FOLLOWER_URI_PREFIX: &str = "x-rincon:";

pub fn spawn(app_handle: tauri::AppHandle) {
    thread::spawn(move || {
        let Ok(mdns) = ServiceDaemon::new() else { return };
        let Ok(receiver) = mdns.browse(SONOS_SERVICE_TYPE) else { return };
        let state = app_handle.state::<AppState>();

        let agent = ureq::builder()
            .timeout_connect(Duration::from_millis(SONOS_REQUEST_TIMEOUT_MS))
            .timeout_read(Duration::from_millis(SONOS_REQUEST_TIMEOUT_MS))
            .timeout_write(Duration::from_millis(SONOS_REQUEST_TIMEOUT_MS))
            .build();

        let mut speakers: BTreeMap<String, Ipv4Addr> = BTreeMap::new();

        loop {
            while let Ok(event) = receiver.try_recv() {
                match event {
                    ServiceEvent::ServiceResolved(info) => {
                        let name = info.get_fullname().to_string();
                        match info.get_addresses_v4().into_iter().next() {
                            Some(ip) => { speakers.insert(name, ip); }
                            None => { speakers.remove(&name); }
                        }
                    }
                    ServiceEvent::ServiceRemoved(_, fullname) => { speakers.remove(&fullname); }
                    _ => {}
                }
            }

            let polled: Vec<MediaStats> = speakers.values()
                .filter_map(|ip| poll_speaker(&agent, *ip))
                .collect();

            let media = polled.iter().find(|m| m.media_status == "playing")
                .or_else(|| polled.iter().find(|m| m.media_status == "paused"))
                .cloned()
                .unwrap_or_default();
            *state.sonos_media.lock().unwrap() = media;

            thread::sleep(Duration::from_millis(SONOS_POLL_INTERVAL_MS));
        }
    });
}

fn poll_speaker(agent: &ureq::Agent, ip: Ipv4Addr) -> Option<MediaStats> {
    let transport = soap_call(agent, ip, "GetTransportInfo")?;
    let position = soap_call(agent, ip, "GetPositionInfo")?;
    parse_speaker(&transport, &position)
}

fn soap_call(agent: &ureq::Agent, ip: Ipv4Addr, action: &str) -> Option<String> {
    let url = format!("http://{}:{}/MediaRenderer/AVTransport/Control", ip, SONOS_PORT);
    let body = format!(
        "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:{action} xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID></u:{action}></s:Body></s:Envelope>"
    );
    agent.post(&url)
        .set("Content-Type", "text/xml; charset=\"utf-8\"")
        .set("SOAPACTION", &format!("\"urn:schemas-upnp-org:service:AVTransport:1#{}\"", action))
        .send_string(&body)
        .ok()?
        .into_string()
        .ok()
}

fn element_text(doc: &roxmltree::Document, name: &str) -> String {
    doc.descendants()
        .find(|n| n.is_element() && n.tag_name().name() == name)
        .and_then(|n| n.text())
        .unwrap_or("")
        .trim()
        .to_string()
}

fn parse_speaker(transport_xml: &str, position_xml: &str) -> Option<MediaStats> {
    let transport = roxmltree::Document::parse(transport_xml).ok()?;
    let status = match element_text(&transport, "CurrentTransportState").as_str() {
        "PLAYING" | "TRANSITIONING" => "playing",
        "PAUSED_PLAYBACK" => "paused",
        _ => return None,
    };

    let position = roxmltree::Document::parse(position_xml).ok()?;
    if element_text(&position, "TrackURI").starts_with(GROUP_FOLLOWER_URI_PREFIX) {
        return None;
    }
    let metadata = element_text(&position, "TrackMetaData");
    if metadata.is_empty() || metadata == DIDL_NOT_IMPLEMENTED {
        return None;
    }

    let didl = roxmltree::Document::parse(&metadata).ok()?;
    let mut title = element_text(&didl, "title");
    let creator = element_text(&didl, "creator");
    let album = element_text(&didl, "album");
    let stream_content = element_text(&didl, "streamContent");
    if !stream_content.is_empty() && creator.is_empty() {
        title = stream_content;
    }
    if title.is_empty() {
        return None;
    }

    Some(MediaStats {
        media_status: status.to_string(),
        media_name: any_ascii(&title),
        media_author: any_ascii(&creator),
        media_album: any_ascii(&album),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn transport(state: &str) -> String {
        format!("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetTransportInfoResponse xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><CurrentTransportState>{state}</CurrentTransportState><CurrentTransportStatus>OK</CurrentTransportStatus><CurrentSpeed>1</CurrentSpeed></u:GetTransportInfoResponse></s:Body></s:Envelope>")
    }

    fn position(uri: &str, escaped_didl: &str) -> String {
        format!("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetPositionInfoResponse xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><Track>1</Track><TrackDuration>0:03:20</TrackDuration><TrackMetaData>{escaped_didl}</TrackMetaData><TrackURI>{uri}</TrackURI><RelTime>0:00:10</RelTime><AbsTime>NOT_IMPLEMENTED</AbsTime><RelCount>2147483647</RelCount><AbsCount>2147483647</AbsCount></u:GetPositionInfoResponse></s:Body></s:Envelope>")
    }

    fn didl(title: &str, creator: &str, album: &str, stream_content: &str) -> String {
        format!("&lt;DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns:r=\"urn:schemas-rinconnetworks-com:metadata-1-0/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"&gt;&lt;item id=\"-1\" parentID=\"-1\" restricted=\"true\"&gt;&lt;dc:title&gt;{title}&lt;/dc:title&gt;&lt;dc:creator&gt;{creator}&lt;/dc:creator&gt;&lt;upnp:album&gt;{album}&lt;/upnp:album&gt;&lt;r:streamContent&gt;{stream_content}&lt;/r:streamContent&gt;&lt;/item&gt;&lt;/DIDL-Lite&gt;")
    }

    fn media(status: &str, name: &str, author: &str, album: &str) -> MediaStats {
        MediaStats {
            media_status: status.to_string(),
            media_name: name.to_string(),
            media_author: author.to_string(),
            media_album: album.to_string(),
        }
    }

    #[test]
    fn stopped_speaker_yields_none() {
        let stopped_transport = transport("STOPPED");
        let stopped_position = "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetPositionInfoResponse xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><Track>0</Track><TrackDuration>0:00:00</TrackDuration><TrackMetaData></TrackMetaData><TrackURI></TrackURI><RelTime>0:00:00</RelTime><AbsTime>NOT_IMPLEMENTED</AbsTime><RelCount>2147483647</RelCount><AbsCount>2147483647</AbsCount></u:GetPositionInfoResponse></s:Body></s:Envelope>";
        assert_eq!(parse_speaker(&stopped_transport, stopped_position), None);
    }

    #[test]
    fn playing_track() {
        let pos = position("x-sonos-spotify:abc", &didl("Song", "Artist", "Album", ""));
        assert_eq!(parse_speaker(&transport("PLAYING"), &pos), Some(media("playing", "Song", "Artist", "Album")));
        assert_eq!(parse_speaker(&transport("TRANSITIONING"), &pos), Some(media("playing", "Song", "Artist", "Album")));
    }

    #[test]
    fn paused_track() {
        let pos = position("x-sonos-spotify:abc", &didl("Song", "Artist", "Album", ""));
        assert_eq!(parse_speaker(&transport("PAUSED_PLAYBACK"), &pos), Some(media("paused", "Song", "Artist", "Album")));
    }

    #[test]
    fn radio_uses_stream_content() {
        let pos = position("x-sonosapi-stream:s1", &didl("Radio Station", "", "", "Artist - Song"));
        assert_eq!(parse_speaker(&transport("PLAYING"), &pos), Some(media("playing", "Artist - Song", "", "")));
    }

    #[test]
    fn group_follower_yields_none() {
        let pos = position("x-rincon:RINCON_123456", &didl("Song", "Artist", "Album", ""));
        assert_eq!(parse_speaker(&transport("PLAYING"), &pos), None);
    }

    #[test]
    fn ampersand_title_is_unescaped() {
        let pos = position("x-sonos-spotify:abc", &didl("Rock &amp;amp; Roll", "Artist &amp;amp; Friends", "Album", ""));
        assert_eq!(
            parse_speaker(&transport("PLAYING"), &pos),
            Some(media("playing", "Rock & Roll", "Artist & Friends", "Album"))
        );
    }
}
