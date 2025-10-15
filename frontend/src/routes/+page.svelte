<script lang="ts">
    import ScanResultsTable from "$lib/components/ScanResultsTable.svelte";
    import { ScanResults } from "$lib/scanResults.svelte";

    let scanResults: ScanResults | undefined = $state(undefined);

    async function pair() {
        scanResults = await ScanResults.setupFromBLEDevice();
    }

    async function dummy() {
        scanResults = await ScanResults.setupDummy();
    }
</script>
<div>
    <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={pair}>Pair</button>
    <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={dummy}>Dummy</button>
</div>
{#if scanResults}
    <ScanResultsTable {scanResults} />
{/if}
