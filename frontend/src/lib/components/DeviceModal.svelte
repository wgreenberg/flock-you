<script lang="ts">
    import { DeviceType, WifiFrameType, type DeviceSummary } from "$lib/scanResults.svelte";
    import DeviceSummaryItems from "./DeviceSummaryItems.svelte";
    import Map from "./Map.svelte";
    import Modal from "./Modal.svelte";
    import SummaryIcon from "./SummaryIcon.svelte";
    let { device, currentLocation, showModal = $bindable() }: {
        device?: DeviceSummary,
        currentLocation?: GeolocationCoordinates,
        showModal: boolean,
    } = $props();
</script>

{#if device}
<Modal bind:showModal >
    <div class="flex flex-col">
        <div class="flex flex-row align-bottom">
            {#if 'ssid' in device}
                <SummaryIcon
                    type={device.frameType == WifiFrameType.beacon ? "wifiBeacon" : "wifiProbe"}
                    label={`Wifi ${device.frameType === WifiFrameType.beacon ? "Beacon" : "Probe Request"}`}
                    />
            {:else}
                <SummaryIcon
                    type="ble"
                    label="Bluetooth LE"
                    />
            {/if}
        </div>
        <DeviceSummaryItems {device} />
        <Map {device} {currentLocation} />
    </div>
</Modal>
{/if}
