//sensor holder for Melexis Infrared thermometer in TO-39 metal can
// digi-Key part MLX90614ESF-BCI-000-TU-ND
//designed for pan and tilt 

$fn=36;

difference(){
union(){   
//baseplate    
translate([-1.5,0,0])cube([63,55,3], center=true);  
// extension for strain relief ties    
translate([4.5,0,0])cube([73,28,3], center=true);   
// cube for IRT    
translate([-23,0,20/2])cube([20,20,21], center=true);  

} 
union(){
//hole for IRT
translate([-20,0,11])rotate([0,90,0])cylinder(r = 9.7/2, h = 40, center=true);
//hole for M3 set screw    
translate([-26,-7.5,11])rotate([90,0,0]) cylinder(r = 1.5, h = 55, center=true);
//cutout
cylinder(r = 12, h = 6, center=true);   
// corner mounting holes on base plate    
translate([60/2-7,54/2-10,0])cylinder(h=4,r=3/2,center=true);
translate([60/2-7,-54/2+6,0])cylinder(h=4,r=3/2,center=true);  
translate([-54/2+0,-54/2+6,0])cylinder(h=4,r=3/2,center=true); 
translate([-54/2+0,+54/2-10,0])cylinder(h=4,r=3/2,center=true); 
//holes for zip tie strain relief 
translate([60/2+10/2,-8,0])cylinder(h=4,r=4/2,center=true); 
translate([60/2+10/2,8,0])cylinder(h=4,r=4/2,center=true);    
}
}

