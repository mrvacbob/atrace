#include "raytrace.h"
#include "tests.h"

/*static primitive **lots_of_spheres(int *pi)
{
	primitive **prims = new primitive*[500];

	prims[0] = new plane_prim(vector3(0,1,0), 4.4);
	prims[1] = new sphere(point3(2, .8, 3), 2.5);
	prims[2] = new sphere(point3(-5.5, -.5, 7), 2);
	prims[3] = new sphere(point3(0,5,5), .1);
	prims[4] = new sphere(point3(-3, 5, 1), .1);
	prims[5] = new sphere(point3(-1.5, -3.8, 1), 1.5);
	prims[6] = new plane_prim(vector3(.4,0,-1), 12);
	prims[7] = new plane_prim(vector3(0,-1,0), 7.4);
	
	prims[0]->mat.c = color(.4,.3,.3);
	prims[0]->mat.diffuse = 1;
	
	prims[1]->mat.c = color(.7,.7,1);
	prims[1]->mat.diffuse = .2;
	prims[1]->mat.reflect = 0;
	prims[1]->mat.transmittance = .8;
	prims[1]->mat.dielectric = true;
	prims[1]->mat.refractive_index = 1.31;
	
	prims[2]->mat.c = color(.7,.7,1.);
	prims[2]->mat.reflect = .5;
	prims[2]->mat.clear_reflect = false;
	prims[2]->mat.diffuse = .1;
	
	prims[3]->mat.c = color(.4,.4,.4);
	prims[3]->light = true;
	
	prims[4]->mat.c = color(.6,.6,.6);
	prims[4]->light = true;
	
	prims[5]->mat.c = color(1,.4,.4);
	prims[5]->mat.diffuse = .2;
	
	prims[6]->mat.c = color(.5,.3,.5);
	prims[6]->mat.diffuse = .6;
	
	prims[7]->mat.c = color(.4,.7,.7);
	prims[7]->mat.diffuse = .5;
	
	int primi = 8;
	for (int x = 0; x < 8; x++)
		for (int y = 0; y < 7; y++) {
			prims[primi] = new sphere(vector3(-4.5 + x * 1.5, -4.3 + y * 1.5, 10), .3);
			prims[primi]->mat.diffuse = .6;
			prims[primi]->mat.c = color(.3,1,.4);
			primi++;
		}
			
	*pi = primi;
	return prims;
}
*/

static void set_color(primitive *p, color4 c)
{
	p->mat.textures[0].tex = new flat_texture(c);
	p->mat.texcount = 1;
}

static primitive **checkerboard_scene(int *pi)
{
	primitive **prims = new primitive*[6];

	prims[0] = new plane_prim(vector3(0,1,0), 2);
	prims[1] = new plane_prim(vector3(0,0,-1), 15);
	prims[2] = new sphere(vector3(-2.4, 1.8, 3), 2);
	prims[3] = new sphere(vector3(2.4, 1.8, 3), 2);
	prims[4] = new sphere(vector3(0, 10, -1), .2);
	prims[5] = new sphere(vector3(0, -.2, 7), 2);

	set_color(prims[0], color4(.7,.7,.7));
	prims[0]->mat.diffuse = 1;
	
	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = new checkerboard_texture(color4(1,.1,.1),color4(.1,.15,1));
	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = new image_texture("caro.png", true);
	prims[1]->mat.textures[1].uScale = prims[1]->mat.textures[1].vScale = 50.;
	prims[1]->mat.textures[1].vShift = 12*50;
	prims[1]->mat.textures[1].uShift = 12*50;
	prims[1]->mat.diffuse = .7;
	
	set_color(prims[2], color4(.7,.9,.7));
	prims[2]->mat.reflect = 0;
	prims[2]->med.transmittance = .8;
	prims[2]->mat.diffuse = .1;
	
	set_color(prims[3], color4(.7,.7,.7));
	prims[3]->mat.reflect = 0;
	prims[3]->med.transmittance = 1;
	prims[3]->mat.diffuse = .1;
	prims[3]->med.refractive_index = 1.1;
	prims[3]->mat.dielectric = true;
	
	set_color(prims[4], color4(5,5,5));
	prims[4]->light = true;
	
	set_color(prims[5], color4(.75,.75,.4));
	//prims[5]->mat.textures[0].tex = new checkerboard_texture(color(1,.1,.1),color(.1,.15,1));
	//prims[5]->mat.texcount++;
	prims[5]->mat.diffuse =.1;
	prims[5]->mat.reflect =.5;
	prims[5]->mat.clear_reflect = false;

	*pi = 6;
	
	return prims;
}

int main (int argc, char * const argv[]) {
	size_t w = 1280, h = 960;
	camera cam;
	int primi;
	//primitive **prims = lots_of_spheres(&primi);
	
	primitive **prims = checkerboard_scene(&primi);
	
	raytracer tr(prims,primi);
	
	cam.origin = point3(0,0,-5);
	cam.screen.origin = point3(0,0,0);
	cam.screen.w = 8;
	cam.screen.h = 6;
	
	//run_tests();
		
	tr.render(w,h,cam)->write_to_bmp("scene.bmp");
		
	return 0;
}
