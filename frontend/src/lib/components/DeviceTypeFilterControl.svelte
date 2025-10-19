<script lang="ts">
    import SummaryIcon from "./SummaryIcon.svelte";

    let {
        showWifiBeacon = $bindable(true),
        showWifiProbe = $bindable(true),
        showBLE = $bindable(true),
    }: {
        showWifiBeacon: boolean,
        showWifiProbe: boolean,
        showBLE: boolean,
    } = $props();
</script>

<div class="flex flex-col p-2 w-40">
    <label class="flex flex-row justify-between">
        <div class="flex flex-row">
            <SummaryIcon
                type="wifiBeacon"
                label="Wifi Beacon"
                />
        </div>
        <input type="checkbox" bind:checked={showWifiBeacon}>
    </label>
    <label class="flex flex-row justify-between">
        <div class="flex flex-row">
            <SummaryIcon
                type="wifiProbe"
                label="Wifi Probe"
                />
        </div>
        <input type="checkbox" bind:checked={showWifiProbe}>
    </label>
    <label class="flex flex-row justify-between">
        <div class="flex flex-row">
            <SummaryIcon
                type="ble"
                label="Bluetooth LE"
                />
        </div>
        <input type="checkbox" bind:checked={showBLE}>
    </label>
</div>
