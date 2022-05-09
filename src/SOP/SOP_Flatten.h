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
 * Flatten SOP
 */


#ifndef __SOP_Flatten_h__
#define __SOP_Flatten_h__

#include <SOP/SOP_Node.h>

namespace HDK_Sample {
class SOP_Flatten : public SOP_Node
{
public:
	     SOP_Flatten(OP_Network *net, const char *name, OP_Operator *op);
            ~SOP_Flatten() override;

    /// This method is created so that it can be called by handles.  It only
    /// cooks the input group of this SOP.  The geometry in this group is
    /// the only geometry manipulated by this SOP.
    OP_ERROR                     cookInputGroups(OP_Context &context, 
						int alone = 0) override;

    static PRM_Template		 myTemplateList[];
    static OP_Node		*myConstructor(OP_Network*, const char *,
							    OP_Operator *);

protected:
    /// Update disable and hidden states of parameters based on the value
    /// of other parameters.
    bool                         updateParmsFlags() override;

    const char                  *inputLabel(unsigned idx) const override;

    /// Method to cook geometry for the SOP
    OP_ERROR                     cookMySop(OP_Context &context) override;

    /// This method is used to generate geometry for a "guide".  It does
    ///	not have to be defined.
    OP_ERROR                     cookMyGuide1(OP_Context &context) override;

private:
    void	getGroups(UT_String &str){ evalString(str, "group", 0, 0); }
    fpreal	DIST(fpreal t)		{ return evalFloat("dist", 0, t); }
    int		DIRPOP()		{ return evalInt("usedir", 0, 0); }
    int		ORIENT()		{ return evalInt("orient", 0, 0); }
    fpreal	NX(fpreal t)		{ return evalFloat("dir", 0, t); }
    fpreal	NY(fpreal t)		{ return evalFloat("dir", 1, t); }
    fpreal	NZ(fpreal t)		{ return evalFloat("dir", 2, t); }

    /// This is the group of geometry to be manipulated by this SOP and cooked
    /// by the method "cookInputGroups".
    const GA_PointGroup *myGroup;
};
} // End HDK_Sample namespace

#endif
