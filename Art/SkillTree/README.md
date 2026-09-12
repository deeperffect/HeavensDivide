# Ascension panel

Generated with the built-in image-generation tool for the skill tree menu. Source: `AscensionPanel.png`; imported UI texture: `/Game/HeavensDivide/Blueprints/UI/SkillTree/AscensionPanel`.

The page pairs this texture with the existing menu's Cinzel Medium/Regular fonts, InkHover button brush, and MenuBrushStroke dividers. The node symbols remain vector graphics for crisp zooming.

Final generation prompt:

> Create a finished game UI background texture, landscape 1536x1024. Japanese dark fantasy sumi-e ink on charcoal paper. Nearly black quiet center occupying 80 percent of image, low contrast warm gray dry brush grain and faint swirling smoke only near perimeter. Two very subtle ghostly ink brush arcs suggesting intertwined souls at far right edge, no distinct creatures. Fine distressed ivory double-line rectangular frame inset 25 pixels from all edges, uneven hand-painted corners. Elegant restrained monochrome charcoal and aged ivory, tactile grunge, no blue, no gold. This is background behind a dense skill tree: center must remain very dark and empty for readable interface. NO text, NO lettering, NO icons, NO nodes, NO connections. Opaque full-bleed image.

Reimport with `Tools/import_skill_tree_art.py` through Unreal's Python commandlet. The texture uses the UI group, UI compression, and no mipmaps. Native widget references keep the artwork and fonts included in cooked builds.

Border containment revision (built-in image editing tool):

> Edit target: supplied UI panel. Preserve its dark charcoal ink texture, quiet dark center, faint soul arcs on right, landscape proportions, thin distressed ivory double rectangular frame in same position. Required precise correction: ALL texture, grunge, smoke, brushwork and corner splatters must be strictly INSIDE the rectangular ivory border. Remove every mark outside it. Outside the outermost frame line must be perfectly uniform solid black (#000000), with no grain, splatters, texture or decoration whatsoever, including all four corners. Make frame corners closed clean right angles, distress only inward. Do not add text, icons, or nodes. Keep interior composition and subdued contrast.

Buttons have no resting or disabled texture. Hover uses the white brush and black lettering for contrast.
