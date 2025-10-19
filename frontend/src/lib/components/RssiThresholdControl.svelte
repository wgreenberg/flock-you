<script lang="ts">
    let { rssiThreshold = $bindable(-60) }: { rssiThreshold: number } = $props();
    const min = -120;
</script>

<label>
    <span>RSSI Threshold</span>
   	<input type="number" bind:value={rssiThreshold} {min} max="0" />
   	<input type="range" bind:value={rssiThreshold} {min} max="0" />
</label>
