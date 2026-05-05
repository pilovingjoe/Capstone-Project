// Modular Box Generator

$fn = 64;

wall        = 2;      wall_thin   = 1.5;
wall_bottom = 2.0;    gap         = 0.0;
rim_depth   = 2.0;    rim_thick   = 1.0;
lip_height  = 0.6;    lip_depth   = 0.25;
lip_offset  = rim_depth - lip_height;

actualBoardThickness = 6;
shelf_thick = 4;      shelf_width = 3;

// Compartment dims
c_length = 55; c_width = 29; c_height = 7.5;
m_length = 32; m_width = 18; m_height = 16;

// Cutout params
usb_w = 9.5; usb_h = 4.5; usb_spacing = 6.0;
btn_d = 12.5; btn_y_offset = 13.0;
pin_slit_w = 3; pin_slit_l = 40.0;

// style
ext_r = 3.5;
vent_w = 1.8; vent_l = 10.0; vent_depth = 1.6; vent_count = 4; vent_spacing = 5.0;

ctrl_full_w = c_width + wall * 2;
ctrl_full_l = c_length + wall_thin + wall;
mod_full_w  = m_width + wall * 2;
mod_full_l  = m_length + wall;

combo_h = max(c_height, m_height);

ctrl_y_start = wall_thin;
ctrl_y_end   = ctrl_y_start + c_length;
mod_y_start  = ctrl_y_end;
mod_y_end    = mod_y_start + m_length;
total_l      = ctrl_y_end + wall + m_length;

usb_z       = wall_bottom + combo_h - rim_depth - actualBoardThickness;
shelf_top_z = usb_z;

shared_wall_open_h = shelf_top_z - wall_bottom;


module rounded_box(w, l, h, r) {
    hull()
        for (dx = [r, w-r], dy = [r, l-r])
            translate([dx, dy, 0]) cylinder(r=r, h=h);
}

module vent_slots_strip(panel_l, panel_h, count, spacing) {
    total   = (count - 1) * spacing;
    start_y = (panel_l - total) / 2;
    slot_z  = wall_bottom + (panel_h - vent_l) / 2;
    for (i = [0 : count - 1])
        translate([0, start_y + i * spacing, slot_z])
            cube([vent_depth + 1, vent_w, vent_l]);
}

module shelf(dir, fixed, a0, a1) {
    sz = shelf_top_z; sw = shelf_width; st = shelf_thick;
    if (dir == "py") polyhedron(
        points=[[a0,fixed,sz-st],[a0,fixed,sz],[a0,fixed+sw,sz],
                [a1,fixed,sz-st],[a1,fixed,sz],[a1,fixed+sw,sz]],
        faces=[[0,1,2],[3,5,4],[0,3,4,1],[1,4,5,2],[0,2,5,3]]);
    if (dir == "ny") polyhedron(
        points=[[a0,fixed,sz-st],[a0,fixed,sz],[a0,fixed-sw,sz],
                [a1,fixed,sz-st],[a1,fixed,sz],[a1,fixed-sw,sz]],
        faces=[[0,2,1],[3,4,5],[0,1,4,3],[1,2,5,4],[0,3,5,2]]);
}

module pin_slits(z_start, z_h) {
    for (dx = [0, c_width - pin_slit_w])
        translate([wall + dx, ctrl_y_start + (c_length - pin_slit_l)/2, z_start])
            cube([pin_slit_w, pin_slit_l, z_h]);
}

module btn_hole(x, y) {
    translate([x, y, -1]) linear_extrude(10) rotate([0,0,30]) circle(d=btn_d, $fn=6);
}

module l_rim_ring(h, extra=0) {
    rt = rim_thick + extra;
    difference() {
        union() {
            translate([wall - rt/2, wall_thin - rt/2, 0])
                cube([c_width + rt, c_length + rt, h]);
            translate([wall - rt/2, ctrl_y_end - rt/2, 0])
                cube([m_width + rt, m_length + rt, h]);
        }
        // Hollow out ctrl bay interior
        translate([wall + rt/2, wall_thin + rt/2, -1])
            cube([c_width - rt, c_length - rt, h + 2]);
        // Hollow out module bay interior
        translate([wall + rt/2, ctrl_y_end + rt/2, -1])
            cube([m_width - rt, m_length - rt, h + 2]);
        // Remove the inner corner where they overlap (the step in the L)
        // so the ring only exists along the actual L perimeter
        translate([wall + m_width + rt/2, ctrl_y_end - rt/2, -1])
            cube([c_width - m_width, m_length + rt, h + 2]);
    }
}


module combined_base() {
    total_h = combo_h + wall_bottom;

    difference() {
        union() {
            rounded_box(ctrl_full_w, ctrl_full_l, total_h, ext_r);
            rounded_box(mod_full_w,  ctrl_full_l + mod_full_l - wall, total_h, ext_r);
        }

        translate([wall, ctrl_y_start, wall_bottom])
            cube([c_width, c_length, combo_h + 1]);

        translate([wall, mod_y_start, wall_bottom])
            cube([m_width, m_length, combo_h + 1]);

        translate([wall, ctrl_y_end - 0.1, wall_bottom])
            cube([m_width, wall + 0.2, shared_wall_open_h]);

        translate([0, 0, combo_h + wall_bottom - rim_depth])
            l_rim_ring(rim_depth + 1);

       
        translate([0, 0, combo_h + wall_bottom - rim_depth])
            l_rim_ring(lip_height, extra = lip_depth * 2);

        for (dx = [wall + c_width/2 - usb_spacing/2 - usb_w,
                   wall + c_width/2 + usb_spacing/2])
            translate([dx, -1, usb_z]) cube([usb_w, wall_thin + 2, usb_h]);

hull() {
    translate([wall, -1, usb_z - usb_h - 4])
        cube([c_width, wall_thin + 2, 0.01]);
    translate([wall + c_width/2 - c_width/3, -1, usb_z - usb_h - 4 + usb_h/1.5])
        cube([c_width/1.5, wall_thin + 2, 0.01]);
}
        pin_slits(-1, wall_bottom + rim_depth + 2);
        
        for (i = [0:2])
            translate([wall + (c_width - (c_width - pin_slit_w*4))/2, ctrl_y_start + (c_length - pin_slit_l)/2 + pin_slit_l/4 + i * pin_slit_l/4, -1])
                cube([c_width - pin_slit_w*4, pin_slit_w, wall_bottom + 2]);

        translate([-1, 0, 0])
            vent_slots_strip(total_l, combo_h, vent_count, vent_spacing);
        translate([ctrl_full_w - vent_depth, 0, 0])
            vent_slots_strip(ctrl_full_l, combo_h, vent_count, vent_spacing);
        translate([0, total_l - vent_depth, 0])
            vent_slots_strip(mod_full_w, combo_h, vent_count, vent_spacing);
    }

    shelf("py", ctrl_y_start, wall, wall + c_width);

  
    shelf("ny", ctrl_y_end, wall, wall + c_width);
    
    translate([wall, ctrl_y_end - 7, shelf_top_z])
        cube([4, 7, m_height - rim_depth - (shelf_top_z - wall_bottom)]);
        polyhedron(
            points=[
                [wall,     ctrl_y_end - 7, shelf_top_z],
                [wall + 4, ctrl_y_end - 7, shelf_top_z],
                [wall,     ctrl_y_end,     shelf_top_z],
                [wall + 4, ctrl_y_end,     shelf_top_z],
                [wall,     ctrl_y_end - 7, shelf_top_z - 4],
                [wall + 4, ctrl_y_end - 7, shelf_top_z - 1]
            ],
            faces=[
                [0,1,3,2],
                [0,4,5,1],
                [0,2,4],
                [1,5,3],
                [2,3,5,4]
            ]
        );
    translate([wall + c_width - 4, ctrl_y_end - 7, wall_bottom])
        cube([4, 7, m_height - rim_depth]);
    
difference() {
    translate([wall + m_width - 8, mod_y_end - 13, wall_bottom])
        cube([8, 13, m_height - rim_depth]);
    translate([wall + m_width - 8 + wall, mod_y_end - 13 + wall, wall_bottom])
        cube([8 - wall*2, 13 - wall, m_height - rim_depth + 1]);
}
}

module combined_lid() {
    translate([0, 0, wall_bottom + rim_depth]) mirror([0,0,1]) {
        difference() {
            union() {

                rounded_box(ctrl_full_w, ctrl_full_l, wall_bottom, ext_r);
                rounded_box(mod_full_w,  ctrl_full_l + mod_full_l - wall, wall_bottom, ext_r);


                translate([0, 0, wall_bottom])
                    l_rim_ring(rim_depth);


                translate([0, 0, wall_bottom + lip_offset])
                    l_rim_ring(lip_height, extra = -lip_depth * 2);
            }


            btn_hole(wall + c_width/2 - usb_spacing/2 - usb_w/2, ctrl_y_start + btn_y_offset);
            btn_hole(wall + c_width/2 + usb_spacing/2 + usb_w/2, ctrl_y_start + btn_y_offset);
            btn_hole(wall + c_width/2,                            ctrl_y_start + 28);

            lines = ["TrevorBly", "TheKing"];
            for (i = [0 : len(lines) - 1])
                translate([wall + c_width/2,
                           ctrl_y_start + c_length/2 + c_length/3 - i*5,
                           wall_bottom/4])
                    linear_extrude(wall_bottom)
                        mirror([1,0,0])
                            text(lines[i], size=3.5,
                                 font="Liberation Sans:style=Bold",
                                 halign="center", valign="middle");


            pin_slits(-1, wall_bottom + rim_depth + 2);

            translate([-1, 0, 0])
                vent_slots_strip(total_l, rim_depth - 0.5, 3, vent_spacing);
            translate([ctrl_full_w - vent_depth, 0, 0])
                vent_slots_strip(ctrl_full_l, rim_depth - 0.5, 3, vent_spacing);
        }
    }
}


combined_base();
translate([ctrl_full_w + 25, 0, 0]) combined_lid();
