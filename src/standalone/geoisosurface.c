// When executed this simple programm generates an ISO surface 
// and saves it to a Houdini geometry file names sphere.bgeo
#include <GU/GU_Detail.h>	// Houdini geometry utility library
#include <stddef.h>

namespace HDK_Sample {
	static float
	densityFunction(const UT_Vector3 &P, void *data)
	{
		// Return the signed distance to the unit sphere
		return 1 - P.length();
	}
}

int
main(int argc, char *argv[])
{
	GU_Detail		gdp;
	UT_BoundingBox	bounds;

	// Evaluate the iso-surface inside this bounding box
	bounds.setBounds(-1, -1, -1, 1, 1, 1);

	// Create an iso-surface
	gdp.polyIsoSurface(HDK_Sample::densityFunction, NULL, bounds, 20, 20, 20);

	// Save to sphere.bgeo
	gdp.save("sphere.bgeo", NULL);

	return 0;
}