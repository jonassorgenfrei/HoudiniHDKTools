/*
 * Copyright (c) 2022
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 */
#include "SOP_PolyClip.h"
#include "SOP_PolyClip.proto.h"

#include <GA/GA_ElementWrangler.h>
#include <GA/GA_SplittableRange.h>
#include <GU/GU_Detail.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_DSOVersion.h>

using namespace HDK_Sample;

/// This is the internal name of the SOP type.
/// It isn't allowed to be the same as any other SOP's type name.
const UT_StringHolder SOP_PolyClip::theSOPTypeName("hdk_polyclip");

void
newSopOperator(OP_OperatorTable *table)
{
    OP_Operator *op =
	new OP_Operator(
		SOP_PolyClip::theSOPTypeName,	// Internal name
		"PolyClip",	    		// GUI name
		SOP_PolyClip::myConstructor,	// Op Constructr
		SOP_PolyClip::buildTemplates(),	// Parameter Definition
		1,				// Min # of Inputs
		1,				// Max # of Inputs
		nullptr,			// Local variables
		0				// flags
	);

    table->addOperator(op);
}

static const char *theDsFile = R"THEDSFILE(
{
    name	hdk_polyclip

    parm {
	name	"group"
	label	"Group"
	type	string
	default	{ "" }
	parmtag	{ "script_action" "import soputils\nkwargs['geometrytype'] = (hou.geometryType.Primitives,)\nkwargs['inputindex'] = 0\nsoputils.selectGroupParm(kwargs)" }
	parmtag	{ "script_action_help" "Select primitives from an available viewport." }
	parmtag	{ "script_action_icon" "BUTTONS_reselect" }
    }
    groupsimple {
        name    "clipplane"
        label   "Clip Plane"

	parm {
	    name	"origin"
	    label	"Origin"
	    type	vector
	    size	3
	    default	{ "0" "0" "0" }
	}
	parm {
	    name	"normal"
	    label	"Normal"
	    type	vector
	    size	3
	    default	{ "0" "1" "0" }
	}
    }
}
)THEDSFILE";

PRM_Template *
SOP_PolyClip::buildTemplates()
{
    static PRM_TemplateBuilder templ("SOP_PolyClip.C", theDsFile);
    if (templ.justBuilt())
    {
	templ.setChoiceListPtr("group", &SOP_Node::primGroupMenu);
    }
    return templ.templates();
}

OP_Node *
SOP_PolyClip::myConstructor(
	OP_Network *net, const char *name, OP_Operator *entry)
{
    return new SOP_PolyClip(net, name, entry);
}

SOP_PolyClip::SOP_PolyClip(
	OP_Network *net, const char *name, OP_Operator *entry)
    : SOP_Node(net, name, entry)
{
}

SOP_PolyClip::~SOP_PolyClip()
{
}

OP_ERROR
SOP_PolyClip::cookMySop(OP_Context &context)
{
    return cookMyselfAsVerb(context);
}

const char *
SOP_PolyClip::inputLabel(unsigned idx) const
{
    switch (idx)
    {
	case 0: return "Input Geometry";
	default: return "Invalid Source";
    }
}

class SOP_PolyClipVerb : public SOP_NodeVerb
{
public:
    SOP_PolyClipVerb() {}
    ~SOP_PolyClipVerb() override {}

    SOP_NodeParms *allocParms() const override
	{ return new SOP_PolyClipParms(); }

    UT_StringHolder name() const override
   	 { return SOP_PolyClip::theSOPTypeName; }

    CookMode cookMode(const SOP_NodeParms *parms) const override
	{ return COOK_DUPLICATE; }

    void cook(const CookParms &cookparms) const override;
};

// register a verb for our SOP
static SOP_NodeVerb::Register<SOP_PolyClipVerb> theSOPPolyClipVerb;

const SOP_NodeVerb *
SOP_PolyClip::cookVerb() const 
{ 
    return theSOPPolyClipVerb.get();
}

void
SOP_PolyClipVerb::cook(const SOP_NodeVerb::CookParms &cookparms) const
{
    auto &&sopparms = cookparms.parms<SOP_PolyClipParms>();
    GU_Detail *gdp = cookparms.gdh().gdpNC();

    UT_Vector3 org = sopparms.getOrigin();
    UT_Vector3 nml = sopparms.getNormal();

    // returns true if the point is clipped by the plane
    auto isClipped = [&](GA_Offset pt) -> bool {
	return dot(nml, gdp->getPos3(pt) - org) < 0;
    };

    // clip distance along edge (normalized to [0, 1]) from pt0
    auto clippedDist = [&](GA_Offset pt0, GA_Offset pt1) -> fpreal {
	UT_Vector3 pos0 = gdp->getPos3(pt0);
	UT_Vector3 pos1 = gdp->getPos3(pt1);
	fpreal denom = dot(nml, pos1 - pos0);
	if (!denom)
	    return 0.0;

	return SYSclamp(dot(nml, org - pos0) / denom, 0.0, 1.0);
    };

    GOP_Manager gop;
    const GA_PrimitiveGroup *group = nullptr;
    if (sopparms.getGroup().isstring())
    {
	group = gop.parsePrimitiveGroups(sopparms.getGroup(),
					 GOP_Manager::GroupCreator(gdp, false));
    }

    // identify polygons that need to be removed and ones to be recreated
    // as clipped polygons
    GA_PrimitiveGroup *rm_polys = gdp->newInternalPrimitiveGroup();
    GA_PrimitiveGroup *clipped_polys = gdp->newInternalPrimitiveGroup();
    UTparallelFor(
	GA_SplittableRange(gdp->getPrimitiveRange(group)),
	[&](const GA_Range &r)
	{
	    for (GA_Iterator it(r); !it.atEnd(); ++it)
	    {
		GA_Offset pr = *it;

		// we only support closed polygons
		if (gdp->getPrimitiveTypeId(pr) != GA_PRIMPOLY
		   || !gdp->getPrimitiveClosedFlag(pr))
		    continue;

		exint clipped = 0;
		exint nvtx = gdp->getPrimitiveVertexCount(pr);
		for (exint i = 0; i < nvtx; ++i)
		{
		    GA_Offset vtx = gdp->getPrimitiveVertexOffset(pr, i);
		    GA_Offset pt = gdp->vertexPoint(vtx);
		    if (isClipped(pt))
			++clipped;
		}
		if (clipped)
		{
		    rm_polys->addOffset(pr);
		    if (clipped != nvtx)
		    {
			// some of this polygon should remain after clipping
			clipped_polys->addOffset(pr);
		    }
		}
	    }
	});

    // recreate clipped polygons
    GA_Topology &topo = gdp->getTopology();
    GA_PointWrangler pt_wrangler(*gdp, GA_PointWrangler::INCLUDE_P);
    GA_PrimitiveWrangler prim_wrangler(*gdp);
    GA_VertexWrangler vtx_wrangler(*gdp);

    GA_OffsetArray vert0s;
    GA_OffsetArray vert1s;
    UT_FloatArray dists;
    UT_IntArray poly_start_pos;
    UT_Map<std::pair<GA_Offset, GA_Offset>, GA_Offset> cuts;
    for (GA_Iterator it(gdp->getPrimitiveRange(clipped_polys));
	 !it.atEnd(); ++it)
    {
	GA_Offset pr = *it;
	exint nvtx = gdp->getPrimitiveVertexCount(pr);

	vert0s.clear();
	vert1s.clear();
	dists.clear();
	poly_start_pos.clear();

	// iterate through the vertices of the current primitive
	for (exint i0 = 0; i0 < nvtx; ++i0)
	{
	    GA_Offset vtx0 = gdp->getPrimitiveVertexOffset(pr, i0);
	    GA_Offset pt0 = gdp->vertexPoint(vtx0);
	    bool clipped0 = isClipped(pt0);
	    if (!clipped0)
	    {
		// keep the point if it is not clipped
		vert0s.append(vtx0);
		vert1s.append(GA_INVALID_OFFSET);
		dists.append(0);
	    }

	    exint i1 = (i0 + 1) % nvtx;
	    GA_Offset vtx1 = gdp->getPrimitiveVertexOffset(pr, i1);
	    GA_Offset pt1 = gdp->vertexPoint(vtx1);
	    bool clipped1 = isClipped(pt1);

	    if (clipped0 != clipped1)
	    {
		if (clipped0)
		    poly_start_pos.append(vert0s.entries());

		// one of the edges points has been clipped, find
		// the location of the cut

		// sort the points to ensure we find the same cut
		// location regardless of the point order
		if (pt1 < pt0)
		{
		    UTswap(vtx0, vtx1);
		    UTswap(pt0, pt1);
		}

		vert0s.append(vtx0);
		vert1s.append(vtx1);
		dists.append(clippedDist(pt0, pt1));
	    }
	}

	// create clipped polygons
	exint num_verts = vert0s.entries();
	exint num_new_polys = poly_start_pos.entries();
	for (exint i = 0; i < num_new_polys; ++i)
	{
	    exint start = poly_start_pos(i);
	    exint end = poly_start_pos((i + 1) % num_new_polys);

	    exint nvtx = end - start;
	    if(nvtx <= 0)
		nvtx += num_verts;

	    // create new polygon
	    GA_Offset start_vtx;
	    GA_Offset new_pr =
		gdp->appendPrimitivesAndVertices(
			GA_PRIMPOLY, 1, nvtx, start_vtx, true);
	    prim_wrangler.copyAttributeValues(new_pr, pr);

	    // set the individual vertices
	    for (exint v = 0; v < nvtx; ++v)
	    {
		GA_Offset vtx = start_vtx + v;

		exint idx = (start + v) % num_verts;
		GA_Offset vtx0 = vert0s(idx);
		GA_Offset vtx1 = vert1s(idx);

		GA_Offset pt0 = gdp->vertexPoint(vtx0);
		if (vtx1 == GA_INVALID_OFFSET)
		{
		    // a vertex from the original polygon
		    topo.wireVertexPoint(vtx, gdp->vertexPoint(vtx0));
		    vtx_wrangler.copyAttributeValues(vtx, vtx0);
		}
		else
		{
		    // a vertex produced when cutting an edge
		    fpreal dist = dists(idx);

		    GA_Offset pt1 = gdp->vertexPoint(vtx1);
		    auto key = std::make_pair(pt0, pt1);
		    auto it = cuts.find(key);
		    if (it != cuts.end())
		    {
			// reuse existing point for this edge
			topo.wireVertexPoint(vtx, it->second);
		    }
		    else
		    {
			// append a new point for this edge
			GA_Offset pt = gdp->appendPoint();
			pt_wrangler.lerpAttributeValues(pt, pt0, pt1, dist);
			topo.wireVertexPoint(vtx, pt);
			cuts.emplace(key, pt);
		    }
		    vtx_wrangler.lerpAttributeValues(vtx, vtx0, vtx1, dist);
		}
	    }
	}
    }

    // destroy the clipped polygons and the any points that would become
    // unconnected after removing the polygons
    gdp->destroyPrimitiveOffsets(gdp->getPrimitiveRange(rm_polys), true);

    // destroy our temporary groups
    gdp->destroyPrimitiveGroup(rm_polys);
    gdp->destroyPrimitiveGroup(clipped_polys);
}
