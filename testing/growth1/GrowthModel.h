/*
 * GrowthModel.h
 *
 *  Created on: Oct 13, 2017
 *      Author: andy
 */

#ifndef TESTING_GROWTH1_GROWTHMODEL_H_
#define TESTING_GROWTH1_GROWTHMODEL_H_

#include "MxModel.h"

struct GrowthModel : public MxModel {

    GrowthModel();

    /**
     * Evaluate the force functions,
     */
    virtual HRESULT calcForce(TrianglePtr* triangles, uint32_t len) ;

};


#endif /* TESTING_GROWTH1_GROWTHMODEL_H_ */
