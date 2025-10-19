<script lang="ts">
    import type { DeviceSummary } from "$lib/scanResults.svelte";
    import SummaryIcon from "./SummaryIcon.svelte";

    let { device }: { device: DeviceSummary } = $props();

    let isMacRandomized = $derived([0x2, 0x6, 0xA, 0xE].includes(device.mac[0] & 0x0F));
</script>

{#if 'ssid' in device}
    <span>SSID: {device.ssid ? device.ssid : "N/A"}</span>
{:else}
    <span>Name: {device.deviceName ? device.deviceName : "N/A"}</span>
{/if}
<span class="flex flex-row">
    <p class="mr-2">MAC Address:</p>
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
<span>MAC Manufacturer Info: {device.getOUIManufacturer() || "N/A"}</span>
{#if 'manufacturerData' in device}
    <span>BLE Manufacturer Info: {device.getBLEManufacturerNames() || "N/A"}</span>
{/if}
<span>Last RSSI: {device.lastRSSI().rssi}</span>
