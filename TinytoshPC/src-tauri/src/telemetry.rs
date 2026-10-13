//! This computer's load, for the device's PC monitor screen.

use sysinfo::{Disks, Networks, System};

// Free space moves slowly, and listing the disks is the costly part of a sample.
const DISK_REFRESH_EVERY_SAMPLES: u32 = 30;

#[derive(serde::Serialize, Clone, Copy, Default)]
pub(crate) struct Sample {
    pub(crate) cpu_percent: f32,
    pub(crate) net_down_kb: u64,
    pub(crate) mem_percent: f64,
    pub(crate) disk_percent: u64,
}

pub(crate) struct Telemetry {
    system: System,
    disks: Disks,
    networks: Networks,
    samples: u32,
    disk_percent: u64,
}

impl Telemetry {
    pub(crate) fn new() -> Self {
        Telemetry {
            system: System::new(),
            disks: Disks::new(),
            networks: Networks::new_with_refreshed_list(),
            samples: 0,
            disk_percent: 0,
        }
    }

    /// Call once a second: the network figure is what arrived since the last call.
    pub(crate) fn sample(&mut self) -> Sample {
        self.system.refresh_cpu_usage();
        self.system.refresh_memory();
        self.networks.refresh();

        if self.samples.is_multiple_of(DISK_REFRESH_EVERY_SAMPLES) {
            self.disks.refresh_list();
            self.disk_percent = self
                .disks
                .list()
                .iter()
                .find(|disk| disk.mount_point().to_str() == Some("/"))
                .or_else(|| self.disks.list().iter().find(|disk| disk.mount_point().to_str() == Some("C:\\")))
                .filter(|disk| disk.total_space() > 0)
                .map_or(0, |disk| (disk.total_space() - disk.available_space()) * 100 / disk.total_space());
        }
        self.samples = self.samples.wrapping_add(1);

        Sample {
            cpu_percent: self.system.global_cpu_usage(),
            net_down_kb: self.networks.values().map(|network| network.received()).sum::<u64>() / 1024,
            mem_percent: self.system.used_memory() as f64 / self.system.total_memory() as f64 * 100.0,
            disk_percent: self.disk_percent,
        }
    }
}
