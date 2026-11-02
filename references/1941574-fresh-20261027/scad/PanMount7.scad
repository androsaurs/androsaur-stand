// Pan Tilt Base
// baseplate
xdim=55+6;
ydim=45-3;
zdim=6;

zcylinder=12;  // height of cylinder

hubdiam=8.4;   // diameter of hub on servo wheel
wheeldiam=21;  // diam of servo wheel
wheelthick=2.2;  // thickness of servo wheel

$fn=72;


difference(){
    union(){
    translate([0,0,10/2-3])cube([xdim,ydim,10], center=true);  // baseplate  
    translate([40/2-10,0,12])cylinder(h=10.0,r=(ydim)/2,center=true); // cylinder
       
    } 
    union(){
      translate([40/2-10,0,14.5])cylinder(h=10,r=(40-3)/2,center=true); // cylinder cutout
      translate([40/2-10,0,0])cylinder(h=30,r=14/2,center=true); // spline hole  
      translate([0,0,-1.5])cube([55,20.4,3],center=true);       // recess
      translate([0,0,10/2-2.5])cube([41,20.4,11],center=true);  // cutout 
      translate([0,0,2])cube([55,2.4,4],center=true);         // nub
      // servo mounting holes  
      translate([55/2-5.5/2,10/2,0])cylinder(h=zcylinder,r=2.0/2,center=true); 
      translate([55/2-5.5/2,-10/2,0])cylinder(h=zcylinder,r=2.0/2,center=true); 
      translate([-55/2+5.5/2,10/2,0])cylinder(h=zcylinder,r=2.0/2,center=true); 
      translate([-55/2+5.5/2,-10/2,0])cylinder(h=zcylinder,r=2.0/2,center=true);  
      // side holes for tilt mount
      translate([-18,0,2])rotate([90.0,0])cylinder(h=ydim+10,r=2.5/2,center=true);
      translate([18,0,2])rotate([90.0,0])cylinder(h=ydim+10,r=2.5/2,center=true);  
        
    }
}

