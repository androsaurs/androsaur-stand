// Pan Tilt Base for std servo
// HK=hobby king 15269
// baseplate
xdim=85;
ydim=42;
zdim=4;

zcylinder=12;  // height of cylinder riser

hubdiam=8.9+0.4;   // diameter of hub on servo wheel (HK =8, hitec=8.9)
wheeldiam=20.7+0.7;  // diam of servo wheel  (HK =20.7, hitec=23.9)
wheelthick=4;  // thickness of servo wheel recess
servospace=15.6; // servo wheel mnt hole spacing (HK =15.6, hitec=17.6)
$fn=72;

module roundedcube(xdim ,ydim ,zdim,rdim){
    hull(){
    translate([rdim,rdim,0])cylinder(h=zdim,r=rdim);
    translate([xdim-rdim,rdim,0])cylinder(h=zdim,r=rdim);
    translate([rdim,ydim-rdim,0])cylinder(h=zdim,r=rdim);
    translate([xdim-rdim,ydim-rdim,0])cylinder(h=zdim,r=rdim);
    }
}

difference(){
    union(){
    roundedcube(xdim ,ydim ,zdim,4); // baseplate
    translate([xdim/2,ydim/2,0])cylinder(h=zcylinder,r=(ydim-6+0.4)/2,center=false); // cylinder
    } 
    union(){
      // middle hole  
      translate([xdim/2,ydim/2,0])cylinder(h=zcylinder,r=8.4/2,center=false);  
      // servo wheel recess  
      translate([xdim/2,ydim/2,zcylinder-wheelthick])cylinder(h=wheelthick,r=wheeldiam/2,center=false); // 
      // mount holes for servo wheel  
      translate([xdim/2-servospace/2,ydim/2,2])cylinder(h=zcylinder,r=2/2,center=false); 
      translate([xdim/2+servospace/2,ydim/2,2])cylinder(h=zcylinder,r=2/2,center=false);
      translate([xdim/2,ydim/2-servospace/2,2])cylinder(h=zcylinder,r=2/2,center=false); 
      translate([xdim/2,ydim/2+servospace/2,2])cylinder(h=zcylinder,r=2/2,center=false);
      // mount holes on corners of baseplate  
      translate([6,6,0])cylinder(h=zcylinder,r=4/2,center=false); 
      translate([6,ydim-6,0])cylinder(h=zcylinder,r=4/2,center=false);
      translate([xdim-6,ydim-6,0])cylinder(h=zcylinder,r=4/2,center=false);
      translate([xdim-6,6,0])cylinder(h=zcylinder,r=4/2,center=false);     
        
    }
}

