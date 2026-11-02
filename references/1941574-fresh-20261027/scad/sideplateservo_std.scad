// side panel with servo horn connection 
// HK=hobby king 15269 servo

$fn=72;

module plate(){
difference(){
    union(){
    translate([0,2,0])cube([60,56,3], center=true);  // baseplate  
    translate([-60/2-15/2,60/2-15/2,-1.5])cylinder(h=6,r=15/2,center=true); // plate for horn, tip
    translate([-60/2-15+15/2,60/2-15,-4.5])cube([26,15,6]); // plate for horn  
    
    } 
    union(){
      translate([-5,-5,0])cylinder(h=3,r=(20)/2,center=true); // big cutout
      translate([-18,-60/2+9,0])cylinder(h=3,r=(3.0)/2,center=true); // plate mnt hole 
      translate([18,-60/2+9,0])cylinder(h=3,r=(3.0)/2,center=true); // plate mnt hole 
      translate([60/4+5,60/4+5,0])rotate([0,0,45])cube([60/2,60,3], center=true); // servo horn plate
      translate([-60/2-15/2,60/2-15/2,0])cylinder(h=10,r=8.5/2,center=true); //horn shaft cutout 
      translate([-60/2-15/2+10.5+0.5,60/2-15/2,0])cylinder(h=10,r=2/2,center=true); //servo horn holes 
      translate([-60/2-15/2+17+0.5,60/2-15/2,0])cylinder(h=10,r=2/2,center=true); //servo horn holes   
        
    }
  }
}

//horn parameters
a4h_end_diam = 6;
a4h_center_diam = 11;
a4h_arm_length = 22;
a4h_drill_distance = 13.3;

module horn_arm(h=5)
{
  translate([0,a4h_arm_length-a4h_end_diam/2,0])
  //-- The arm consist of the perimeter of a cylinder and a cube
  hull() {
    cylinder(r=a4h_end_diam/2, h=h, center=true, $fn=20);
    translate([0,1-a4h_arm_length+a4h_end_diam/2,0])
      cube([a4h_center_diam,2,h],center=true);
  }
}

difference(){
    rotate([0,180,0])plate();
    union(){    
       translate([60/2+15/2,60/2-15/2,4]) rotate([0,0,90])horn_arm();
       translate([60/2+15/2,60/2-15/2,4]) cylinder(h=5,r=11/2,center=true); // cyl 
    }
}

