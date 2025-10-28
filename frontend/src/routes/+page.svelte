<script lang="ts">
    import ScanResultsTable from "$lib/components/ScanResultsTable.svelte";
    import { Scanner } from "$lib/scanResults.svelte";
    import NavBar from "$lib/components/NavBar.svelte";
    import { onMount } from "svelte";
    import RecordedResultsTable from "$lib/components/RecordedResultsTable.svelte";

    let scanner: Scanner | undefined = $state(undefined);
    let selection: string = $state('scanner');

    async function pair() {
        await tryWakeLock();
        scanner = await Scanner.setupFromBLEDevice();
    }

    async function dummy() {
        await tryWakeLock();
        scanner = await Scanner.setupDummy();
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
<NavBar bind:selection {scanner} />
<div class="m-6">
    {#if selection === 'scanner'}
        <div>
            <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={pair}>Pair</button>
            <button id="pair" class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={dummy}>Dummy</button>
        </div>
        {#if scanner}
            <ScanResultsTable {scanner} />
        {/if}
    {:else}
        <RecordedResultsTable />
    {/if}
</div>
