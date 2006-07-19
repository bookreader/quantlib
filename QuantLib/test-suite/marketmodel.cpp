/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "marketmodel.hpp"
#include "utilities.hpp"
#include <ql/MarketModels/Products/marketmodelforwards.hpp>
#include <ql/MarketModels/Products/marketmodelcaplets.hpp>
#include <ql/MarketModels/Products/marketmodelforwardsonestep.hpp>
#include <ql/MarketModels/Products/marketmodelcapletsonestep.hpp>
#include <ql/MarketModels/accountingengine.hpp>
#include <ql/MarketModels/forwardrateevolver.hpp>
#include <ql/MarketModels/forwardrateipcevolver.hpp>
#include <ql/MarketModels/exponentialcorrelation.hpp>
#include <ql/MarketModels/abcdvolatility.hpp>
#include <ql/MarketModels/mtbrowniangenerator.hpp>
#include <ql/schedule.hpp>
#include <ql/Calendars/nullcalendar.hpp>
#include <ql/DayCounters/actual365fixed.hpp>
#include <ql/PricingEngines/blackmodel.hpp>
#include <ql/Utilities/dataformatters.hpp>

#if defined(BOOST_MSVC)
#include <float.h>
//namespace { unsigned int u = _controlfp(_EM_INEXACT, _MCW_EM); }
#endif

using namespace QuantLib;
using namespace boost::unit_test_framework;

QL_BEGIN_TEST_LOCALS(MarketModelTest)

#define BEGIN(x) (x+0)
#define END(x) (x+LENGTH(x))

Date todaysDate;
Date endDate;
Array rateTimes, paymentTimes, accruals;
Calendar calendar;
DayCounter dayCounter;

Array todaysForwards, displacements, todaysDiscounts;
std::vector<Volatility> volatilities;

void setup() {

    // times
    calendar = NullCalendar();
    todaysDate = Settings::instance().evaluationDate();
    endDate = todaysDate + 10*Years;
    Schedule dates(calendar, todaysDate, endDate,
                   Semiannual, Following);
    rateTimes = Array(dates.size()-1);
    paymentTimes = Array(rateTimes.size()-1);
    accruals = Array(rateTimes.size()-1);
    dayCounter = Actual365Fixed();

    for (Size i=1; i<dates.size(); ++i)
        rateTimes[i-1] = dayCounter.yearFraction(todaysDate, dates[i]);

    std::copy(rateTimes.begin()+1, rateTimes.end(), paymentTimes.begin());
    
    for (Size i=1; i<rateTimes.size(); ++i)
        accruals[i-1] = rateTimes[i] - rateTimes[i-1];

    // rates
    todaysForwards = Array(paymentTimes.size());
    displacements = Array(paymentTimes.size());
    for (Size i=0; i<todaysForwards.size(); ++i) {
        todaysForwards[i] = 0.03 + 0.0010*i;
        displacements[i] = 0.02 + 0.0005*i;
    }

    todaysDiscounts = Array(rateTimes.size());
    todaysDiscounts[0] = 0.95;
    for (Size i=1; i<rateTimes.size(); ++i)
        todaysDiscounts[i] = todaysDiscounts[i-1] / 
            (1.0+todaysForwards[i-1]*accruals[i-1]);

    // volatilities
    volatilities = std::vector<Volatility>(todaysForwards.size());
    for (Size i=0; i<volatilities.size(); ++i)
        volatilities[i] = 0.10 + 0.005*i;

}


QL_END_TEST_LOCALS(MarketModelTest)


void MarketModelTest::testForwards() {

    BOOST_MESSAGE("Repricing forwards in a LIBOR market model...");

    QL_TEST_SETUP

    Array strikes = todaysForwards + 0.01;

    boost::shared_ptr<MarketModelProduct> product(
         new MarketModelForwards(rateTimes, accruals, paymentTimes, strikes));
    
    EvolutionDescription evolution = product->suggestedEvolution();

    Real longTermCorrelation = 0.5;
    Real beta = 0.2;

    Size factors = todaysForwards.size();

    boost::shared_ptr<PseudoRoot> pseudoRoot(
                       new ExponentialCorrelation(longTermCorrelation, beta,
                                                  volatilities,
                                                  evolution,
                                                  factors,
                                                  todaysForwards,
                                                  displacements));

    unsigned long seed = 42;
    MTBrownianGeneratorFactory generatorFactory(seed);

    evolution.setTerminalMeasure();
    boost::shared_ptr<MarketModelEvolver> evolver(
            new ForwardRateIpcEvolver(pseudoRoot, evolution, generatorFactory));

    Size initialNumeraire = evolution.numeraires().front();
    Real initialNumeraireValue = todaysDiscounts[initialNumeraire];

    AccountingEngine engine(evolver, product, evolution,
                            initialNumeraireValue);
    SequenceStatistics<> stats(product->numberOfProducts());
    Size paths = 10000;

    engine.multiplePathValues(stats, paths);

    std::vector<Real> results = stats.mean();
    std::vector<Real> errors = stats.errorEstimate();
    
    Array expected(todaysForwards.size());
    for (Size i=0; i<expected.size(); ++i)
        expected[i] = (todaysForwards[i]-strikes[i])
            *accruals[i]*todaysDiscounts[i+1];

    for (Size i=0; i<results.size(); ++i) {
        BOOST_MESSAGE(io::ordinal(i+1) << " forward: "
                      << io::rate(results[i])
                      << " +- " << io::rate(errors[i])
                      << "; expected: " << io::rate(expected[i])
                      << "; discrepancy = "
                      << (results[i]-expected[i])/errors[i]
                      << " standard errors");
    }
}

void MarketModelTest::testCaplets() {

    BOOST_MESSAGE("Repricing caplets in a LIBOR market model...");

    QL_TEST_SETUP

    Array strikes = todaysForwards + 0.01;

    boost::shared_ptr<MarketModelProduct> product(
         new MarketModelCaplets(rateTimes, accruals, paymentTimes, strikes));
    
    EvolutionDescription evolution = product->suggestedEvolution();

    Real longTermCorrelation = 0.5;
    Real beta = 0.2;

    Size factors = todaysForwards.size();

    boost::shared_ptr<PseudoRoot> pseudoRoot(
                       new ExponentialCorrelation(longTermCorrelation, beta,
                                                  volatilities,
                                                  evolution,
                                                  factors,
                                                  todaysForwards,
                                                  displacements));

    //boost::shared_ptr<PseudoRoot> pseudoRoot(
    //                   new AbcdVolatility(1.0, 0.0, 0.0, 1.0,
    //                                      volatilities,
    //                                      longTermCorrelation, beta,
    //                                      evolution,
    //                                      factors,
    //                                      todaysForwards,
    //                                      displacements));

    unsigned long seed = 42;
    MTBrownianGeneratorFactory generatorFactory(seed);

    evolution.setTerminalMeasure();
    boost::shared_ptr<MarketModelEvolver> evolver(
            new ForwardRateIpcEvolver(pseudoRoot, evolution, generatorFactory));

    Size initialNumeraire = evolution.numeraires().front();
    Real initialNumeraireValue = todaysDiscounts[initialNumeraire];

    AccountingEngine engine(evolver, product, evolution,
                            initialNumeraireValue);
    SequenceStatistics<> stats(product->numberOfProducts());
    Size paths = 10000;

    engine.multiplePathValues(stats, paths);

    std::vector<Real> results = stats.mean();
    std::vector<Real> errors = stats.errorEstimate();
    
    Array expected(todaysForwards.size());
    for (Size i=0; i<expected.size(); ++i) {
        Time expiry = rateTimes[i];
        expected[i] =
            detail::blackFormula(todaysForwards[i]+displacements[i],
                                 strikes[i]+displacements[i],
                                 volatilities[i]*std::sqrt(expiry), 1)
            *accruals[i]*todaysDiscounts[i+1];
    }

    for (Size i=0; i<results.size(); ++i) {
        BOOST_MESSAGE(io::ordinal(i+1) << " caplet: "
                      << io::rate(results[i])
                      << " +- " << io::rate(errors[i])
                      << "; expected: " << io::rate(expected[i])
                      << "; discrepancy = "
                      << (results[i]-expected[i])/(errors[i] == 0.0 ? 1.0 : errors[i])
                      << " standard errors");
    }
}


void MarketModelTest::testOneStepForwards() {

    BOOST_MESSAGE("Repricing forwards in a single-step full-factor LIBOR market model...");

    QL_TEST_SETUP

    Array strikes = todaysForwards + 0.01;

    boost::shared_ptr<MarketModelProduct> product(
         new MarketModelForwardsOneStep(rateTimes, accruals,
                                        paymentTimes, strikes));
    
    EvolutionDescription evolution = product->suggestedEvolution();

    Real longTermCorrelation = 0.5;
    Real beta = 0.2;

    boost::shared_ptr<PseudoRoot> pseudoRoot(
        new ExponentialCorrelation(longTermCorrelation, beta,
                                   volatilities,
                                   evolution,
                                   todaysForwards.size(),
                                   todaysForwards,
                                   displacements));

    //boost::shared_ptr<PseudoRoot> pseudoRoot(
    //                   new AbcdVolatility(1.0, 0.0, 0.0, 1.0,
    //                                      volatilities,
    //                                      longTermCorrelation, beta,
    //                                      evolution,
    //                                      todaysForwards.size(),
    //                                      todaysForwards,
    //                                      displacements));

    unsigned long seed = 42;
    MTBrownianGeneratorFactory generatorFactory(seed);

    boost::shared_ptr<MarketModelEvolver> evolver(
            new ForwardRateIpcEvolver(pseudoRoot, evolution,
                                                  generatorFactory));

    Size initialNumeraire = evolution.numeraires().front();
    Real initialNumeraireValue = todaysDiscounts[initialNumeraire];

    AccountingEngine engine(evolver, product, evolution,
                            initialNumeraireValue);
    SequenceStatistics<> stats(product->numberOfProducts());
    Size paths = 10000;

    engine.multiplePathValues(stats, paths);

    std::vector<Real> results = stats.mean();
    std::vector<Real> errors = stats.errorEstimate();
    
    Array expected(todaysForwards.size());
    for (Size i=0; i<expected.size(); ++i)
        expected[i] = (todaysForwards[i]-strikes[i])
            *accruals[i]*todaysDiscounts[i+1];

    for (Size i=0; i<results.size(); ++i) {
        BOOST_MESSAGE(io::ordinal(i+1) << " forward: "
                      << io::rate(results[i])
                      << " +- " << io::rate(errors[i])
                      << "; expected: " << io::rate(expected[i])
                      << "; discrepancy = "
                      << (results[i]-expected[i])/errors[i]
                      << " standard errors");
    }
}

void MarketModelTest::testOneStepCaplets() {

    BOOST_MESSAGE("Repricing caplets in a single-step full-factor LIBOR market model...");

    QL_TEST_SETUP

    Array strikes = todaysForwards + 0.01;

    boost::shared_ptr<MarketModelProduct> product(
         new MarketModelCapletsOneStep(rateTimes, accruals,
                                       paymentTimes, strikes));
    
    EvolutionDescription evolution = product->suggestedEvolution();

    Real longTermCorrelation = 0.5;
    Real beta = 0.2;

    boost::shared_ptr<PseudoRoot> pseudoRoot(
                       new ExponentialCorrelation(longTermCorrelation, beta,
                                                  volatilities,
                                                  evolution,
                                                  todaysForwards.size(),
                                                  todaysForwards,
                                                  displacements));

    //boost::shared_ptr<PseudoRoot> pseudoRoot(
    //                   new AbcdVolatility(1.0, 0.0, 0.0, 1.0,
    //                                      volatilities,
    //                                      longTermCorrelation, beta,
    //                                      evolution,
    //                                      todaysForwards.size(),
    //                                      todaysForwards,
    //                                      displacements));

    unsigned long seed = 42;
    MTBrownianGeneratorFactory generatorFactory(seed);

    boost::shared_ptr<MarketModelEvolver> evolver(
            new ForwardRateIpcEvolver(pseudoRoot, evolution, generatorFactory));

    Size initialNumeraire = evolution.numeraires().front();
    Real initialNumeraireValue = todaysDiscounts[initialNumeraire];

    AccountingEngine engine(evolver, product, evolution,
                            initialNumeraireValue);
    SequenceStatistics<> stats(product->numberOfProducts());
    Size paths = 10000;

    engine.multiplePathValues(stats, paths);

    std::vector<Real> results = stats.mean();
    std::vector<Real> errors = stats.errorEstimate();
    
    Array expected(todaysForwards.size());
    for (Size i=0; i<expected.size(); ++i) {
        Time expiry = rateTimes[i];
        expected[i] =
            detail::blackFormula(todaysForwards[i]+displacements[i],
                                 strikes[i]+displacements[i],
                                 volatilities[i]*std::sqrt(expiry), 1)
            *accruals[i]*todaysDiscounts[i+1];
    }

    for (Size i=0; i<results.size(); ++i) {
        BOOST_MESSAGE(io::ordinal(i+1) << " caplet: "
                      << io::rate(results[i])
                      << " +- " << io::rate(errors[i])
                      << "; expected: " << io::rate(expected[i])
                      << "; discrepancy = "
                      << (results[i]-expected[i])/(errors[i] == 0.0 ? 1.0 : errors[i])
                      << " standard errors");
    }
}


test_suite* MarketModelTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Market-model tests");
    suite->add(BOOST_TEST_CASE(&MarketModelTest::testForwards));
    suite->add(BOOST_TEST_CASE(&MarketModelTest::testOneStepForwards));
    suite->add(BOOST_TEST_CASE(&MarketModelTest::testCaplets));
    suite->add(BOOST_TEST_CASE(&MarketModelTest::testOneStepCaplets));
    return suite;
}

