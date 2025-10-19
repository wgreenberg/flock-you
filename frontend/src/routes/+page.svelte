<script lang="ts">
    import ScanResultsTable from "$lib/components/ScanResultsTable.svelte";
    import { Scanner } from "$lib/scanResults.svelte";
    import { onMount } from "svelte";

    let scanResults: Scanner | undefined = $state(undefined);

    async function pair() {
        await tryWakeLock();
        scanResults = await Scanner.setupFromBLEDevice();
    }

    async function dummy() {
        await tryWakeLock();
        scanResults = await Scanner.setupDummy();
    }

    async function tryWakeLock() {
        if (navigator.wakeLock) {
            try {
                await navigator.wakeLock.request('screen');
            } catch(err) {
                console.error(err);
            }
        } else {
            console.warn('no wake lock API found');
        }
    }
</script>
<div>
    <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={pair}>Pair</button>
    <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={dummy}>Dummy</button>
</div>
{#if scanResults}
    <ScanResultsTable {scanResults} />
{/if}
