# Rock atlas generation and color correction

Generated with the built-in imagegen tool. Style/edit reference:
`Art/ReferenceGround/RockStamps_Atlas.png`.
The first pale sheet was rejected after user feedback. Final files are
`RockStamps_BaseDark.png` and `RockStamps_Additional.png`; both use darker
grey-olive faces from the user's reference. Generated alpha is preserved.

```text
Use case: stylized-concept. Asset type: transparent RGBA rock texture atlas for a top-down hand-painted game terrain. Edit/recompose the reference atlas into FOUR NEW and clearly different complete stone slab silhouettes, arranged exactly as a 2 by 2 grid with generous transparent gutters. Keep the reference's muted olive-grey/khaki stone colors, broad brushwork, very subtle painted bevel highlights and shallow dark seams. Top-down orthographic, flush flat floor stones, no perspective or upright walls. Upper-left: a wide uneven pentagonal slab with a chipped corner and one subtle fracture. Upper-right: a long tapered trapezoid with two offset blunt ends. Lower-left: a broad squat hexagonal slab with a shallow concave notch. Lower-right: a long irregular wedge split into two close fitting pieces. Each design fills its quadrant with no crossings between cells. Keep colors and visual detail density consistent with the reference. Low contrast quiet large faces; sparse medium angular facets; no grain, photoreal pores, strong outlines, dirt, grass, flowers, cast shadow, text, UI, border, frame, watermark or extra objects. Transparent background and transparent gaps. The output is a new sibling atlas, not an overwrite of the reference. High-resolution clean painted silhouettes.
```

Color correction of the base silhouettes (built-in imagegen):

```text
Edit the most recent four-rock atlas. The oldest of the three input images, showing a whole grass/dirt ground with embedded stones, is the user's COLOR AND STYLE reference; the two newer pale rock atlases are too light and too yellow. Produce the four NEW silhouettes from the latest atlas in a clean 2x2 grid. Match the reference's DARK muted grey-olive stone, very restrained shading: dominant face color approximately sRGB #555B4C, shadow facets #42483D, sparse subtle bevels #646A58. No cream/yellow borders, pale tan stone, shiny edges or bright highlights. Keep broad quiet angular brushwork and just a few shallow cracks. Straight overhead flat floor stones, not raised boulders. Preserve fully transparent background and gaps, no grass/soil in this sheet. Each rock must lie entirely within its quadrant with at least 6 percent of the quadrant width of transparent padding on every side; no rock touching the center dividing lines. No text, UI, characters, borders, glow, cast shadows, noise or grain. This is final game texture artwork intended to match the dark stones in the user reference, not a bright presentation render.
```

Color correction of the additional silhouettes (built-in imagegen):

```text
Edit only COLOR AND BRIGHTNESS of this four-stone transparent atlas. Preserve EXACTLY its four silhouettes, their arrangement, widths, heights, fractures, bevel shapes and transparency. Darken and desaturate the pale yellow/khaki stone to the muted DARK grey-olive reference palette: dominant stone faces sRGB #555B4C, darker facets #42483D, very restrained bevels #646A58. The final average face color should be around RGB 85,91,76, with no bright cream or yellow highlights. Keep the broad hand-painted facets and shallow cracks, avoid adding grain, pores, soil, grass or cast shadows. Top-down flat floor slabs. Preserve the four objects and transparent gaps exactly; no text, UI or extra objects.
```
