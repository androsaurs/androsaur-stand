//tilt servo holder and plate for std servo
// HK=hobby king 15269 servo

$fn=72;

difference(){
union(){   
//baseplate    
translate([-1.5,0,0])cube([63,55,3], center=true);  
//servo mounts    
translate([24,-6,20/2])cube([10,12,21], center=true);  
translate([-27,-6,11/2])cube([10,12,11], center=true);  
translate([19,9.5,1])rotate([0,0,-90])wsupport(10,10);  
translate([-32,9.5,1])rotate([0,0,-90])wsupport(10,10);   
//shaft holder pillow block  
translate([-11.8,24.5,11])rotate([90,0,0]) cylinder(r = 6, h = 6, center=true);
translate([-11.8,24.5,5])rotate([90,0,0]) cube([12,12,6], center=true);
translate([-30,23.50-2,1])rotate([0,0,0])wsupport(15,6);   
translate([10-6+2.5,27.5,1])rotate([0,0,-180])wsupport(15,6);  
     
} 
union(){
//hole for servo mounts    
translate([23,-7.5,16.5])rotate([90,0,0]) cylinder(r = 1.2, h = 55, center=true);
translate([23,-7.5,6.5])rotate([90,0,0]) cylinder(r = 1.2, h = 55, center=true);
translate([-26,-7.5,16.5])rotate([90,0,0]) cylinder(r = 1.2, h = 55, center=true);
translate([-26,-7.5,6.5])rotate([90,0,0]) cylinder(r = 1.2, h = 55, center=true);
// shaft hole in pillow block
translate([-11.8,0,11])rotate([90,0,0]) cylinder(r = 3.2, h = 56, center=true);  
// circule cutout in baseplate   
cylinder(r = 16, h = 6, center=true);   // circule cutout
// corner mounting holes on base plate    
translate([60/2-7,54/2-10,0])cylinder(h=4,r=3/2,center=true);
translate([60/2-7,-54/2+6,0])cylinder(h=4,r=3/2,center=true);  
translate([-54/2,-54/2+6,0])cylinder(h=4,r=3/2,center=true); 
translate([-54/2,+54/2-10,0])cylinder(h=4,r=3/2,center=true);    
}
}

module wsupport(x,y){
    difference(){
        cube([x,y,x]);
        rotate([0,-45,0]) cube([x*1.7,y,x*1.7]);
    }
}    

