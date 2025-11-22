import { type ScanCategory } from "./scanner.svelte";

export const CATEGORIES: ScanCategory[] = [
    {
        name: "Flock",
        wifi_ssids: [
            "flock", "Flock", "FLOCK",
            "FS Ext Battery", // Flock Safety Extended Battery devices
            "Penguin", // Penguin surveillance devices
            "Pigvision", // Pigvision surveillance systems
        ],
        ouis: [
            // FS Ext Battery devices
            [0x58, 0x8e, 0x81], [0xcc, 0xcc, 0xcc], [0xec, 0x1b, 0xbd], [0x90, 0x35, 0xea],
            [0x04, 0x0d, 0x84], [0xf0, 0x82, 0xc0], [0x1c, 0x34, 0xf1], [0x38, 0x5b, 0x44],
            [0x94, 0x34, 0x69], [0xb4, 0xe3, 0xf9],

            // Flock WiFi devices
            [0x70, 0xc9, 0x4e], [0x3c, 0x91, 0x80], [0xd8, 0xf3, 0xbc], [0x80, 0x30, 0x49],
            [0x14, 0x5a, 0xfc], [0x74, 0x4c, 0xa1], [0x08, 0x3a, 0x88], [0x9c, 0x2f, 0x9d],
            [0x94, 0x08, 0x53], [0xe4, 0xaa, 0xea],
        ],
        ble_names: [
            "flock", "Flock", "FLOCK",
            "FS Ext Battery", // Flock Safety Extended Battery devices
            "Penguin", // Penguin surveillance devices
            "Pigvision", // Pigvision surveillance systems
        ],
        ble_manufacturer_ids: [
            0x09C8, // XUNTONG
        ],
    },
];
