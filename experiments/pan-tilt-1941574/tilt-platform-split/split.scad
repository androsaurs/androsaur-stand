// Partition of micromet's original SCAD, Thingiverse 1941574 (CC BY-SA).
// No scaling, remodeling or hole changes. Ribs stay with their mounting groups.
piece = 0;
regions = [
    [[-40,-35,-1.5],[80,70,3]],
    [[18,-13,1.5],[12,23,25]],
    [[-33,-13,1.5],[12,23,25]],
    [[-40,10,1.5],[80,25,25]]
];
module original() {
    include <../../../references/pan-tilt-1941574/scad-rev/tiltservomount_std.scad>
}
intersection() {
    original();
    translate(regions[piece][0]) cube(regions[piece][1]);
}
