//! What is playing on this computer, for the device's media screen.

use any_ascii::any_ascii;
use tauri::Manager;

use crate::AppState;

#[derive(serde::Serialize, Clone, Copy, Default, PartialEq, Debug)]
#[serde(rename_all = "lowercase")]
pub(crate) enum MediaStatus {
    Playing,
    Paused,
    #[default]
    Stopped,
}

/// The media fields of a stats message. The device's font is ASCII, so text is transliterated.
#[derive(serde::Serialize, Clone, Default, PartialEq, Debug)]
pub(crate) struct MediaStats {
    pub(crate) media_status: MediaStatus,
    pub(crate) media_name: String,
    pub(crate) media_author: String,
    pub(crate) media_album: String,
}

impl MediaStats {
    pub(crate) fn track(status: MediaStatus, title: &str, artist: &str, album: &str) -> Self {
        MediaStats {
            media_status: status,
            media_name: any_ascii(title),
            media_author: any_ascii(artist),
            media_album: any_ascii(album),
        }
    }
}

/// A player reports a title of "Unknown" or nothing at all when it has no track.
fn has_title(title: &str) -> bool {
    let title = title.trim();
    !title.is_empty() && !title.eq_ignore_ascii_case("unknown")
}

/// Local playback wins over Sonos, and playing wins over paused.
pub(crate) fn pick_media(local: &MediaStats, sonos: &MediaStats) -> MediaStats {
    let ranked = [
        (local, MediaStatus::Playing),
        (sonos, MediaStatus::Playing),
        (local, MediaStatus::Paused),
        (sonos, MediaStatus::Paused),
    ];
    ranked
        .iter()
        .find(|(m, status)| m.media_status == *status)
        .map(|(m, _)| (*m).clone())
        .unwrap_or_else(|| local.clone())
}

#[cfg(target_os = "macos")]
pub(crate) fn spawn(app: tauri::AppHandle) {
    use std::thread;
    use std::time::Duration;

    thread::spawn(move || {
        let state = app.state::<AppState>();
        loop {
            // Asked before taking the lock: the answer comes from another process.
            let now_playing = match mediaremote_rs::get_now_playing() {
                Some(info) if has_title(&info.title) => MediaStats::track(
                    if info.playing { MediaStatus::Playing } else { MediaStatus::Paused },
                    info.title.trim(),
                    &info.artist.unwrap_or_default(),
                    &info.album.unwrap_or_default(),
                ),
                _ => MediaStats::default(),
            };
            *state.media_info.lock().unwrap() = now_playing;
            thread::sleep(Duration::from_millis(1000));
        }
    });
}

#[cfg(not(target_os = "macos"))]
pub(crate) fn spawn(app: tauri::AppHandle) {
    use futures::StreamExt;
    use nowhear::{MediaEvent, MediaSource, MediaSourceBuilder};

    fn status_of(playback_state: &impl std::fmt::Debug) -> MediaStatus {
        match format!("{:?}", playback_state).to_lowercase().as_str() {
            "playing" => MediaStatus::Playing,
            "paused" => MediaStatus::Paused,
            _ => MediaStatus::Stopped,
        }
    }

    tauri::async_runtime::spawn(async move {
        let Ok(source) = MediaSourceBuilder::new().build().await else { return };
        let state = app.state::<AppState>();
        let publish = |media: MediaStats| *state.media_info.lock().unwrap() = media;
        let mut current_player: Option<String> = None;

        // The first player with a track becomes the current one; a playing one ends the search.
        macro_rules! adopt_a_player {
            ($stop_at_first:expr) => {
                if let Ok(players) = source.list_players().await {
                    for player_name in players {
                        let Ok(info) = source.get_player(&player_name).await else { continue };
                        let status = status_of(&info.playback_state);
                        let Some(track) = info.current_track else { continue };
                        if !has_title(&track.title) || status == MediaStatus::Stopped {
                            continue;
                        }
                        current_player = Some(player_name.clone());
                        publish(MediaStats::track(status, &track.title, &track.artist.join(", "), &track.album.unwrap_or_default()));
                        if $stop_at_first || status == MediaStatus::Playing {
                            break;
                        }
                    }
                }
            };
        }

        adopt_a_player!(false);

        let Ok(mut stream) = source.event_stream().await else { return };
        while let Some(event) = stream.next().await {
            match event {
                MediaEvent::PositionChanged { .. } => continue,
                MediaEvent::TrackChanged { player_name, .. } | MediaEvent::StateChanged { player_name, .. } => {
                    if let Ok(info) = source.get_player(&player_name).await {
                        let status = status_of(&info.playback_state);
                        let is_current = current_player.as_ref() == Some(&player_name);
                        let titled = info.current_track.as_ref().is_some_and(|track| has_title(&track.title));

                        if status == MediaStatus::Stopped || (!titled && is_current) {
                            if is_current {
                                current_player = None;
                                publish(MediaStats::default());
                            }
                        } else if titled && (status == MediaStatus::Playing || is_current) {
                            if let Some(track) = info.current_track {
                                current_player = Some(player_name.clone());
                                publish(MediaStats::track(status, &track.title, &track.artist.join(", "), &track.album.unwrap_or_default()));
                            }
                        }
                    }
                }
                MediaEvent::PlayerRemoved { player_name } => {
                    if current_player.as_ref() == Some(&player_name) {
                        current_player = None;
                        publish(MediaStats::default());
                    }
                }
                _ => {}
            }

            if current_player.is_none() {
                adopt_a_player!(true);
            }
        }
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    fn with_status(status: MediaStatus, name: &str) -> MediaStats {
        MediaStats { media_status: status, media_name: name.to_string(), ..MediaStats::default() }
    }

    #[test]
    fn pick_media_over_all_status_pairs() {
        use MediaStatus::{Paused, Playing, Stopped};
        let table = [
            (Playing, Playing, "local"),
            (Playing, Paused, "local"),
            (Playing, Stopped, "local"),
            (Paused, Playing, "sonos"),
            (Paused, Paused, "local"),
            (Paused, Stopped, "local"),
            (Stopped, Playing, "sonos"),
            (Stopped, Paused, "sonos"),
            (Stopped, Stopped, "local"),
        ];
        for (l, s, winner) in table {
            let local = with_status(l, "local");
            let sonos = with_status(s, "sonos");
            let expected = if winner == "local" { &local } else { &sonos };
            assert_eq!(&pick_media(&local, &sonos), expected, "local={l:?} sonos={s:?}");
        }
    }

    #[test]
    fn media_serializes_as_the_firmware_expects() {
        let json = serde_json::to_string(&MediaStats::track(MediaStatus::Playing, "Señorita", "A", "")).unwrap();
        assert_eq!(json, r#"{"media_status":"playing","media_name":"Senorita","media_author":"A","media_album":""}"#);
        assert_eq!(serde_json::to_string(&MediaStatus::default()).unwrap(), "\"stopped\"");
    }
}
