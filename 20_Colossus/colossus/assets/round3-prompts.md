
# Round three painted masters — built-in image_gen

## bronze_wear

Use case: stylized-concept. Asset type: seamless painted bronze wear texture for COLOSSUS, a 320x240 demoscene, shipped as 64x64 indexed tile. Flat orthographic material swatch filling the square edge to edge. Dark load-bearing bronze #302820, broad quiet body planes #706048, sparse worn grazing patches #B89868; deep recess #101820. Broad low contrast rubbed areas and a few long scratches, extremely restrained, hand painted, no checker, no noise, no rivets, no text, no border, no lighting gradients, no rendered object. Tileable edges. Large marks that survive reduction to 64x64 and 5-bit RGB.

Saved: `source/bronze-wear-master.png`.

## stone

Use case: stylized-concept. Painted seamless stone floor texture for a quiet monumental plain, square edge-to-edge swatch, reduced to 64x64. Grey brown sedimentary stone #302c28 to #605848, broad horizontal sediment bands and only two or three subdued long cracks. Hand painted, flat top view, low contrast, no pebbles, no checker, no objects, no text, no border, no shadows, no fine noise; tileable, broad shapes for five-bit DAC.

Saved: `source/stone-master.png`.

## dawn_sky

Use case: stylized-concept. Painted panoramic DAWN SKY only, 4:1 composition, no ground, objects, sun disk, stars or text. COLOSSUS demo texture shipped 256x64 indexed. Top cold blue-black #101820 grading through cold #8098A8. Lowest quarter opens a broad restrained warm #E0C098 horizon, very shallow haze, a few quiet long horizontal cloud veils. Upper-left slightly brighter, low contrast hand painting, large soft shapes, no grain. The machine stays cold, only its sky warms.

Saved: `source/dawn-sky-master.png`.

## dawn_matcap

Use case: stylized-concept. Chrome dawn MATCAP lookup, one circular orthographic polished sphere touching all four edges, square composition, outside #283840. Strong wide pale cold sky reflection band #B8D0D8 across upper half, upper-left #D8E0E0 highlight, dark blue ground reflection #283840 lower half with narrow pale warm dawn #E0C098 reflected at horizon ONLY. Large smooth bands, no landscape detail, no text or borders or sparkles. This 64x64 normal lookup must read as cold chrome at 320x240, never gold.

Saved: `source/dawn-matcap-master.png`.

## warm_environment

Use case: stylized-concept. Warm INTERNAL ENVIRONMENT matcap for recessed machinery, square 64x64 normal lookup master. One orthographic circular metal sphere touching four image edges. Deep blue-black #101820 recesses, narrow reflected furnace orange #C06830 at lower third and soft warm grey pale band upper-left. Dark restricted warmth, strong broad bands, no flame silhouettes or scenery, no text, no objects, no border. Outside circle #101820. Quiet hand painted reflection.

Saved: `source/warm-environment-master.png`.

## furnace

Use case: stylized-concept. Painted furnace flow tile, horizontal 2:1, edge to edge. To reduce to 64x32 indexed. A narrow recessed molten orange #C06830 flow across the middle with brighter #E0A060 core and dark #101820 above and below. Slow heavy flowing heat, broad soft striations, not explosive flames. Tileable horizontally, restrained source, no sparks, no machinery, no text, no border.

Saved: `source/furnace-master.png`.

## ember_stamps

Use case: stylized-concept. Ember sprite atlas on pure black background, four orange ember marks arranged vertically in FOUR equal square cells, total aspect 1:4 (or four clearly separated equal cells vertically). Each mark centered: tiny dim round, medium warm elongated, bright pale orange coal, soft dust puff. All have soft edges fading completely to pure black, no trails crossing cells, no other marks, no text. Hand painted stamps for 16x16 reduction per cell and 4-bit palette. Keep compact readable 1–3 pixel appearance at runtime.

Saved: `source/ember-stamps-master.png`.

## dusk_matcap_v3

Use case: stylized-concept. Improved chrome dusk MATCAP normal lookup for COLOSSUS demoscene, square, exactly one circular orthographic polished sphere filling image touching four sides. Dominant broad BRIGHT COLD SKY BAND across upper half #B0C8D8 with upper-left pale #D8E0E0 highlight taking a quarter of sphere. Lower half clearly dark #283840 ground band, base #101820. High contrast large smooth reflection shapes, no orange or bronze, no landscape details, text, grain, sparkle, border, or extra objects. Outside sphere #283840. Reduce to 64x64 indexed, material must read cold chrome at native 320x240.

Saved: `source/dusk-matcap-v3-master.png`.

