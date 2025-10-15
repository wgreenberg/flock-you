<script lang="ts">
    import { type BLESummary } from "$lib/scanResults.svelte";

    let { ble }: { ble: BLESummary } = $props();
</script>

<div class="rounded-lg bg-fuchsia-200 m-2 p-2 flex flex-col">
    <span class="flex">
        <svg width="20px" height="20px" class="mr-1" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
         <path d="M6 7L18 17L12 22V2L18 7L6 17" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
         </svg>
        <h3>Bluetooth LE device</h3>
    </span>
    <span>Name: {ble.device_name ? ble.device_name : "N/A"}</span>
    <span>MAC Address: {ble.mac_address}</span>
</div>
