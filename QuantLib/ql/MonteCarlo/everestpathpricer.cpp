
/*
 * Copyright (C) 2000-2001 QuantLib Group
 *
 * This file is part of QuantLib.
 * QuantLib is a C++ open source library for financial quantitative
 * analysts and developers --- http://quantlib.org/
 *
 * QuantLib is free software and you are allowed to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of it under the conditions stated
 * in the QuantLib License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You should have received a copy of the license along with this file;
 * if not, please email quantlib-users@lists.sourceforge.net
 * The license is also available at http://quantlib.org/LICENSE.TXT
 *
 * The members of the QuantLib Group are listed in the Authors.txt file, also
 * available at http://quantlib.org/group.html
*/

/*! \file everestpathpricer.cpp
    \brief path pricer for European-type Everest option

    \fullpath
    MonteCarlo/%everestpathpricer.cpp

*/

// $Id$

#include "ql/MonteCarlo/everestpathpricer.hpp"
#include "ql/dataformatters.hpp"

namespace QuantLib {

    namespace MonteCarlo {

        EverestPathPricer::EverestPathPricer(double discount):
            discount_(discount) {
            QL_REQUIRE(discount_ > 0.0,
                "EverestPathPricer: discount must be positive");
            isInitialized_ = true;
        }

        double EverestPathPricer::operator()(const MultiPath & path) const {
            int numAssets = path.assetNumber();
            QL_REQUIRE(isInitialized_,
                "EverestPathPricer: pricer not initialized");

            double minPrice = QL_MAX_DOUBLE;
            for(int i = 0; i < numAssets; i++)
                minPrice = QL_MIN(minPrice, QL_EXP(path[i][0]));

            return discount_ * minPrice;
        }

    }

}
