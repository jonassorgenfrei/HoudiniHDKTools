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
 * The Flatten SOP.  This SOP Flattens the geometry onto a plane.
 */

#include "SOP_Flatten.h"

#include <SOP/SOP_Guide.h>
#include <GU/GU_Detail.h>
#include <GA/GA_Iterator.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Matrix3.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_Vector3.h>
#include <SYS/SYS_Math.h>
#include <stddef.h>

using namespace HDK_Sample;

void
newSopOperator(OP_OperatorTable *table)
{   
    // will be located on load 
    table->addOperator(new OP_Operator(
        "hdk_flatten",      // internal name
        "Flatten",          // UI Name
        SOP_Flatten::myConstructor,     // How to build the SOP
        SOP_Flatten::myTemplateList,    // parameters
        1,                              // min number of sources
        1,                              // max number of sources
        NULL,                           // local variables
        0U));                           // flags such as OP_FLAG_GENERATOR
}

static PRM_Name names[] = {
    PRM_Name("usedir",	"Use Direction Vector"),
    PRM_Name("dist",	"Distance"),
};

PRM_Template
SOP_Flatten::myTemplateList[] = {
    PRM_Template(PRM_STRING,    1, &PRMgroupName, 0, &SOP_Node::pointGroupMenu,
				0, 0, SOP_Node::getGroupSelectButton(
						GA_GROUP_POINT)),
    PRM_Template(PRM_FLT_J,	1, &names[1], PRMzeroDefaults, 0,
				   &PRMscaleRange),
    PRM_Template(PRM_TOGGLE,    1, &names[0]),
    PRM_Template(PRM_ORD,	1, &PRMorientName, 0, &PRMplaneMenu),
    PRM_Template(PRM_DIRECTION, 3, &PRMdirectionName, PRMzaxisDefaults),
    PRM_Template(),
};


OP_Node *
SOP_Flatten::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_Flatten(net, name, op);
}

SOP_Flatten::SOP_Flatten(OP_Network *net, const char *name, OP_Operator *op)
    : SOP_Node(net, name, op), myGroup(NULL)
{
    // This indicates that this SOP manually manages its data IDs,
    // so that Houdini can identify what attributes may have changed,
    // e.g. to reduce work for the viewport, or other SOPs that
    // check whether data IDs have changed.
    // By default, (i.e. if this line weren't here), all data IDs
    // would be bumped after the SOP cook, to indicate that
    // everything might have changed.
    // If some data IDs don't get bumped properly, the viewport
    // may not update, or SOPs that check data IDs
    // may not cook correctly, so be *very* careful!
    mySopFlags.setManagesDataIDs(true);

    // Make sure to flag that we can supply a guide geometry
    mySopFlags.setNeedGuide1(true);
}

SOP_Flatten::~SOP_Flatten() {}

bool
SOP_Flatten::updateParmsFlags()
{
    bool changed;

    changed  = enableParm(3, !DIRPOP());
    changed |= enableParm(4,  DIRPOP());

    return changed;
}


OP_ERROR
SOP_Flatten::cookInputGroups(OP_Context &context, int alone)
{
    // The SOP_Node::cookInputPointGroups() provides a good default
    // implementation for just handling a point selection.
    return cookInputPointGroups(
        context, // This is needed for cooking the group parameter, and cooking the input if alone.
        myGroup, // The group (or NULL) is written to myGroup if not alone.
        alone,   // This is true iff called outside of cookMySop to update handles.
                 // true means the group will be for the input geometry.
                 // false means the group will be for gdp (the working/output geometry).
        true,    // (default) true means to set the selection to the group if not alone and the highlight flag is on.
        0,       // (default) Parameter index of the group field
        -1,      // (default) Parameter index of the group type field (-1 since there isn't one)
        true,    // (default) true means that a pointer to an existing group is okay; false means group is always new.
        false,   // (default) false means new groups should be unordered; true means new groups should be ordered.
        true,    // (default) true means that all new groups should be detached, so not owned by the detail;
                 //           false means that new point and primitive groups on gdp will be owned by gdp.
        0        // (default) Index of the input whose geometry the group will be made for if alone.
    );
}


OP_ERROR
SOP_Flatten::cookMySop(OP_Context &context)
{
    // We must lock our inputs before we try to access their geometry.
    // OP_AutoLockInputs will automatically unlock our inputs when we return.
    // NOTE: Don't call unlockInputs yourself when using this!
    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    fpreal now = context.getTime();

    duplicateSource(0, context);

    // These three lines enable the local variable support.  This allows
    // $CR to get the red colour, for example, as well as supporting
    // any varmap created by the Attribute Create SOP.
    // Note that if you override evalVariableValue for your own
    // local variables (like SOP_Star does) it is essential you
    // still call the SOP_Node::evalVariableValue or you'll not
    // get any of the benefit of the built in local variables.

    // The variable order controls precedence for which attribute will be
    // be bound first if the same named variable shows up in multiple
    // places.  This ordering ensures point attributes get precedence.
    setVariableOrder(3, 2, 0, 1);

    // The setCur* functions track which part of the gdp is currently
    // being processed - it is what is used in the evalVariableValue
    // callback as the current point.  The 0 is for the first input,
    // you can have two inputs so $CR2 would get the second input's
    // value.
    setCurGdh(0, myGdpHandle);

    // Builds the lookup table matching attributes to the local variables.
    setupLocalVars();

    // Here we determine which groups we have to work on.  We only
    //	handle point groups.
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT &&
        (!myGroup || !myGroup->isEmpty()))
    {
        UT_AutoInterrupt progress("Flattening Points");

        // Handle all position, normal, and vector attributes.
        // It's not entirely clear what to do for quaternion or transform attributes.
        // We bump the data IDs of the attributes to modify in advance,
        // since we're already looping over them, and we want to avoid
        // bumping them all for each point, in case that's slow.
        UT_Array<GA_RWHandleV3> positionattribs(1);
        UT_Array<GA_RWHandleV3> normalattribs;
        UT_Array<GA_RWHandleV3> vectorattribs;
        GA_Attribute *attrib;
        GA_FOR_ALL_POINT_ATTRIBUTES(gdp, attrib)
        {
            // Skip non-transforming attributes
            if (!attrib->needsTransform())
                continue;

            GA_TypeInfo typeinfo = attrib->getTypeInfo();
            if (typeinfo == GA_TYPE_POINT || typeinfo == GA_TYPE_HPOINT)
            {
                GA_RWHandleV3 handle(attrib);
                if (handle.isValid())
                {
                    positionattribs.append(handle);
                    attrib->bumpDataId();
                }
            }
            else if (typeinfo == GA_TYPE_NORMAL)
            {
                GA_RWHandleV3 handle(attrib);
                if (handle.isValid())
                {
                    normalattribs.append(handle);
                    attrib->bumpDataId();
                }
            }
            else if (typeinfo == GA_TYPE_VECTOR)
            {
                GA_RWHandleV3 handle(attrib);
                if (handle.isValid())
                {
                    vectorattribs.append(handle);
                    attrib->bumpDataId();
                }
            }
        }

        // Iterate over points up to GA_PAGE_SIZE at a time using blockAdvance.
        GA_Offset start;
        GA_Offset end;
        for (GA_Iterator it(gdp->getPointRange(myGroup)); it.blockAdvance(start, end);)
        {
            // Check if user requested abort
            if (progress.wasInterrupted())
                break;

            for (GA_Offset ptoff = start; ptoff < end; ++ptoff)
            {
                // This sets the current point that is beint processed to
                // ptoff.  This means that ptoff will be used for any
                // local variable for any parameter evaluation that occurs
                // after this point.
                // NOTE: Local variables and repeated parameter evaluation
                //       is significantly slower and sometimes more complicated
                //       than having a string parameter that specifies the name
                //       of an attribute whose values should be used instead.
                //       That parameter would only need to be evaluated once,
                //       the attribute could be looked up once, and quickly
                //       accessed; however, a separate point attribute would
                //       be needed for each property that varies per point.
                //       Local variable evaluation isn't threadsafe either,
                //       whereas attributes can be read safely from multiple
                //       threads.
                //
                //       Long story short: *Local variables are terrible.*
                myCurPtOff[0] = ptoff;
                float dist = DIST(now);
                UT_Vector3 normal;
                if (!DIRPOP())
                {
                    switch (ORIENT())
                    {
                        case 0 : // XY Plane
                            normal.assign(0, 0, 1);
                            break;
                        case 1 : // YZ Plane
                            normal.assign(1, 0, 0);
                            break;
                        case 2 : // XZ Plane
                            normal.assign(0, 1, 0);
                            break;
                    }
                }
                else
                {
                    normal.assign(NX(now), NY(now), NZ(now));
                    normal.normalize();
                }

                // Project positions onto the plane by subtracting
                // off the normal component.
                for (exint i = 0; i < positionattribs.size(); ++i)
                {
                    UT_Vector3 p = positionattribs(i).get(ptoff);
                    p -= normal * (dot(normal, p) - dist);
                    positionattribs(i).set(ptoff, p);
                }

                // Normals will now all either be normal or -normal.
                for (exint i = 0; i < normalattribs.size(); ++i)
                {
                    UT_Vector3 n = normalattribs(i).get(ptoff);
                    if (dot(normal, n) < 0)
                        n = -normal;
                    else
                        n = normal;
                    normalattribs(i).set(ptoff, n);
                }

                // Project vectors onto the plane through the origin by
                // subtracting off the normal component.
                for (exint i = 0; i < vectorattribs.size(); ++i)
                {
                    UT_Vector3 v = vectorattribs(i).get(ptoff);
                    v -= normal * dot(normal, v);
                    vectorattribs(i).set(ptoff, v);
                }
            }
        }
    }

    // Clears out all the myCur* variables to ensure we have no
    // stray references.  This ensures that if the parameters are
    // evaluated outside of this cook path they don't try to read
    // possibly stale point pointers.
    resetLocalVarRefs();

    return error();
}

OP_ERROR
SOP_Flatten::cookMyGuide1(OP_Context &context)
{
    const int divs = 5;

    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    float now = context.getTime();

    myGuide1->clearAndDestroy();

    float dist = DIST(now);

    float nx = 0;
    float ny = 0;
    float nz = 1;
    if (!DIRPOP())
    {
        switch (ORIENT())
        {
	    case 0 : // XY Plane
	        nx = 0; ny = 0; nz = 1;
	        break;
	    case 1 : // YZ Plane
	        nx = 1; ny = 0; nz = 0;
	        break;
	    case 2 : // XZ Plane
	        nx = 0; ny = 1; nz = 0;
	        break;
	}
    }
    else
    {
        nx = NX(now); ny = NY(now); nz = NZ(now);
    }

    if (error() >= UT_ERROR_ABORT)
        return error();

    UT_Vector3 normal(nx, ny, nz);
    normal.normalize();

    UT_BoundingBox bbox;
    inputGeo(0, context)->getBBox(&bbox);

    float sx = bbox.sizeX();
    float sy = bbox.sizeY();
    float sz = bbox.sizeZ();
    float size = SYSsqrt(sx*sx + sy*sy + sz*sz);

    float cx = normal.x() * dist;
    float cy = normal.y() * dist;
    float cz = normal.z() * dist;

    myGuide1->meshGrid(divs, divs, size, size);

    UT_Vector3 zaxis(0, 0, 1);
    UT_Matrix3 mat3;
    mat3.dihedral(zaxis, normal);
    UT_Matrix4 xform;
    xform = mat3;
    xform.translate(cx, cy, cz);

    myGuide1->transform(xform);

    return error();
}

const char *
SOP_Flatten::inputLabel(unsigned) const
{
    return "Geometry to Flatten";
}
