// side mounts for tilt with std servo
// HK=hobby king 15269 servo

$fn=72;
difference(){
    union(){
    cube([60,56,3], center=true);  // baseplate  
    translate([-60/2-15/2,60/2-15/2-2,0])cylinder(h=3,r=15/2,center=true); // cylinder
    translate([-60/2-15+15/2,60/2-15-2,-3/2])cube([15,15,3]); // cylinder  
    translate([-60/2-15/2,60/2-15/2-2,4])cylinder(h=5,r=6/2,center=true); //shaft cylinder 
    } 
    union(){
      translate([-7,-5,0])cylinder(h=3,r=(20)/2,center=true); // big cutout
      translate([-18,-60/2+5+2,0])cylinder(h=3,r=(3.0)/2,center=true); // mnt hole 
      translate([18,-60/2+5+2,0])cylinder(h=3,r=(3.0)/2,center=true); // mnt hole 
      translate([55/4+5,55/4+5,0])rotate([0,0,45])cube([65/2,65,3], center=true);  
        
    }
}
