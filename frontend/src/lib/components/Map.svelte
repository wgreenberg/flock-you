<script lang="ts">
    import { DefaultMarker, MapLibre, Marker, Popup, type LngLatLike } from 'svelte-maplibre';
    import { type LngLatBoundsLike } from 'maplibre-gl';
    import type { DeviceSummary } from '$lib/scanResults.svelte';

    let { device, currentLocation }: {
        device: DeviceSummary,
        currentLocation: GeolocationCoordinates,
    } = $props();

    interface RSSIMarker {
        lngLat: LngLatLike,
        color: string,
        label: string,
    }

    const COLORS = [
        'bg-red-600',
        'bg-orange-600',
        'bg-amber-500',
        'bg-yellow-400',
        'bg-green-700',
    ];
    function getRSSIColor(rssi: number): string {
        const MIN = -100;
        const MAX = 0;
        const nColors = COLORS.length;
        const stepSize = Math.abs(MAX - MIN) / nColors;
        let n = MIN;
        for (let i=0; i<nColors; i++) {
            if (rssi >= n && rssi < n + stepSize) {
                return COLORS[i];
            }
            n += stepSize;
        }
        return COLORS[0];
    }

    let coords: RSSIMarker[] = device.rssis.map(rssi => {
        if (rssi.location) {
            const loc = rssi.location;
            const date = new Date(rssi.timestamp);
            return {
                lngLat: {
                    lng: loc.coords.longitude,
                    lat: loc.coords.latitude,
                },
                color: getRSSIColor(rssi.rssi),
                label: `${date.toLocaleTimeString()}: ${rssi.rssi}`,
            };
        }
        return undefined;
    }).filter(coord => !!coord);
    let currentLocationLngLat = $derived({
        lng: currentLocation.longitude,
        lat: currentLocation.latitude,
    });
</script>

<MapLibre
  style="https://basemaps.cartocdn.com/gl/positron-gl-style/style.json"
  class="relative aspect-[9/16] max-h-[70vh] w-full sm:aspect-video sm:max-h-full"
  zoom={17}
  center={coords[0].lngLat}
  standardControls
>
    {#each coords as { lngLat, color, label }}
        <Marker
            {lngLat}
            class={"h-8 w-8 rounded-full border border-gray-200 text-black shadow-2xl focus:outline-2 focus:outline-black " + color}
            >
          <Popup offset={[0, -10]}>
            <div class="text-lg font-bold">{label}</div>
          </Popup>
        </Marker>
    {/each}
    <Marker
        lngLat={currentLocationLngLat}
        class="h-8 w-8 rounded-full border border-gray-200 bg-blue-400 text-black shadow-2xl focus:outline-2 focus:outline-black"
        />
</MapLibre>
