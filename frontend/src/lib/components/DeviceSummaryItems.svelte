<script lang="ts">
    import type { DeviceSummary } from "$lib/scanResults.svelte";
    import SummaryIcon from "./SummaryIcon.svelte";

    let { device }: { device: DeviceSummary } = $props();

    let isMacRandomized = $derived([0x2, 0x6, 0xA, 0xE].includes(device.mac[0] & 0x0F));
</script>

{#if 'ssid' in device}
    <span><b>SSID:</b> {device.ssid ? device.ssid : "N/A"}</span>
{:else}
    <span><b>Name:</b> {device.deviceName ? device.deviceName : "N/A"}</span>
{/if}
<span class="flex flex-row">
    <p class="mr-2"><b>MAC Address:</b></p>
    {#if isMacRandomized}
        <SummaryIcon
            type="dice"
            label={device.macToString()}
            labelClass="font-mono"
            />
    {:else}
        <p class="font-mono">{device.macToString()}</p>
    {/if}
</span>
<span><b>MAC Manufacturer Info:</b> {device.getOUIManufacturer() || "N/A"}</span>
{#if 'manufacturerData' in device}
    <span><b>BLE Manufacturer Info:</b> {device.getBLEManufacturerNames() || "N/A"}</span>
{/if}
<span><b>Last RSSI:</b> {device.lastRSSI().rssi}</span>
