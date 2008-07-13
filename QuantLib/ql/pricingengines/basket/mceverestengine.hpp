/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file mceverestengine.hpp
    \brief Monte Carlo engine for Everest options
*/

#ifndef quantlib_mc_everest_engine_hpp
#define quantlib_mc_everest_engine_hpp

#include <ql/instruments/everestoption.hpp>
#include <ql/pricingengines/mcsimulation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/exercise.hpp>

namespace QuantLib {

    template <class RNG = PseudoRandom, class S = Statistics>
    class MCEverestEngine : public EverestOption::engine,
                            public McSimulation<MultiVariate,RNG,S> {
      public:
        typedef typename McSimulation<MultiVariate,RNG,S>::path_generator_type
            path_generator_type;
        typedef typename McSimulation<MultiVariate,RNG,S>::path_pricer_type
            path_pricer_type;
        typedef typename McSimulation<MultiVariate,RNG,S>::stats_type
            stats_type;
        MCEverestEngine(const boost::shared_ptr<StochasticProcessArray>&,
                        Size maxTimeStepsPerYear,
                        bool brownianBridge,
                        bool antitheticVariate,
                        bool controlVariate,
                        Size requiredSamples,
                        Real requiredTolerance,
                        Size maxSamples,
                        BigNatural seed);
        void calculate() const {

            McSimulation<MultiVariate,RNG,S>::calculate(requiredTolerance_,
                                                        requiredSamples_,
                                                        maxSamples_);
            results_.value = this->mcModel_->sampleAccumulator().mean();

            if (RNG::allowsErrorEstimate) {
                results_.errorEstimate =
                    this->mcModel_->sampleAccumulator().errorEstimate();
            }

            Real notional = arguments_.notional;
            DiscountFactor discount = endDiscount();
            results_.yield = results_.value/(notional * discount) - 1.0;
        }
      private:
        DiscountFactor endDiscount() const;
        // McEverest implementation
        TimeGrid timeGrid() const;
        boost::shared_ptr<path_generator_type> pathGenerator() const {

            Size numAssets = processes_->size();

            TimeGrid grid = timeGrid();
            typename RNG::rsg_type gen =
                RNG::make_sequence_generator(numAssets*(grid.size()-1),seed_);

            return boost::shared_ptr<path_generator_type>(
                         new path_generator_type(processes_,
                                                 grid, gen, brownianBridge_));
        }
        boost::shared_ptr<path_pricer_type> pathPricer() const;

        // data members
        boost::shared_ptr<StochasticProcessArray> processes_;
        Size maxTimeStepsPerYear_;
        Size requiredSamples_;
        Size maxSamples_;
        Real requiredTolerance_;
        bool brownianBridge_;
        BigNatural seed_;
    };


    class EverestMultiPathPricer : public PathPricer<MultiPath> {
      public:
        explicit EverestMultiPathPricer(Real notional,
                                        Rate guarantee,
                                        DiscountFactor discount);
        Real operator()(const MultiPath& multiPath) const;
      private:
        Real notional_;
        Rate guarantee_;
        DiscountFactor discount_;
    };


    // template definitions

    template<class RNG, class S>
    inline MCEverestEngine<RNG,S>::MCEverestEngine(
                   const boost::shared_ptr<StochasticProcessArray>& processes,
                   Size maxTimeStepsPerYear,
                   bool brownianBridge,
                   bool antitheticVariate,
                   bool controlVariate,
                   Size requiredSamples,
                   Real requiredTolerance,
                   Size maxSamples,
                   BigNatural seed)
    : McSimulation<MultiVariate,RNG,S>(antitheticVariate, controlVariate),
      processes_(processes), maxTimeStepsPerYear_(maxTimeStepsPerYear),
      requiredSamples_(requiredSamples), maxSamples_(maxSamples),
      requiredTolerance_(requiredTolerance),
      brownianBridge_(brownianBridge), seed_(seed) {
        registerWith(processes_);
    }

    template <class RNG, class S>
    inline TimeGrid MCEverestEngine<RNG,S>::timeGrid() const {
        Time residualTime = processes_->time(
                                       this->arguments_.exercise->lastDate());
        Size steps = static_cast<Size>(maxTimeStepsPerYear_*residualTime);
        return TimeGrid(residualTime, std::max<Size>(steps, 1));
    }

    template <class RNG, class S>
    inline DiscountFactor MCEverestEngine<RNG,S>::endDiscount() const {
        boost::shared_ptr<GeneralizedBlackScholesProcess> process =
            boost::dynamic_pointer_cast<GeneralizedBlackScholesProcess>(
                                                      processes_->process(0));
        QL_REQUIRE(process, "Black-Scholes process required");

        return process->riskFreeRate()->discount(
                                             arguments_.exercise->lastDate());
    }

    template <class RNG, class S>
    inline boost::shared_ptr<typename MCEverestEngine<RNG,S>::path_pricer_type>
    MCEverestEngine<RNG,S>::pathPricer() const {

        return boost::shared_ptr<
                         typename MCEverestEngine<RNG,S>::path_pricer_type>(
                              new EverestMultiPathPricer(arguments_.notional,
                                                         arguments_.guarantee,
                                                         endDiscount()));
    }

}


#endif