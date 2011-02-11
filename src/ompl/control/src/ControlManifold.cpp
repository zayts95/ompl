/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2010, Rice University
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "ompl/control/ControlManifold.h"
#include "ompl/util/Exception.h"
#include <boost/thread/mutex.hpp>
#include <set>

/// @cond IGNORE
namespace ompl
{
    namespace control
    {

        static void namesList(int op, const std::string &name1, const std::string &name2 = "")
        {
            static boost::mutex lock;
            static std::set<std::string> used;
            boost::mutex::scoped_lock slock(lock);

            if (op == 1) // add
            {
                if (used.find(name1) != used.end())
                    throw Exception("Control manifold name '" + name1 + "' already in use. Manifold names must be unique.");
                used.insert(name1);
            }
            else
                if (op == 2) // remove
                {
                    std::set<std::string>::iterator pos = used.find(name1);
                    if (pos == used.end())
                        throw Exception("No control manifold with name '" + name1 + "' exists.");
                    used.erase(pos);
                }
                else
                    if (op == 3 && name1 != name2) // replace
                    {
                        std::set<std::string>::iterator pos = used.find(name1);
                        if (pos == used.end())
                            throw Exception("No control manifold with name '" + name1 + "' exists.");
                        if (used.find(name2) != used.end())
                            throw Exception("Control manifold name '" + name2 + "' already in use. Manifold names must be unique.");
                        used.erase(pos);
                        used.insert(name2);
                    }
        }
    }
}
/// @endcond

ompl::control::ControlManifold::ControlManifold(const base::StateManifoldPtr &stateManifold) : stateManifold_(stateManifold)
{
    name_ = "Control[" + stateManifold_->getName() + "]";
    namesList(1, name_);
}

ompl::control::ControlManifold::~ControlManifold(void)
{
    namesList(2, name_);
}

const std::string& ompl::control::ControlManifold::getName(void) const
{
    return name_;
}

void ompl::control::ControlManifold::setName(const std::string &name)
{
    namesList(3, name_, name);
    name_ = name;
}

void ompl::control::ControlManifold::setup(void)
{
}

bool ompl::control::ControlManifold::canPropagateBackward(void) const
{
    return true;
}

double* ompl::control::ControlManifold::getValueAddressAtIndex(Control *control, const unsigned int index) const
{
    return NULL;
}

void ompl::control::ControlManifold::printControl(const Control *control, std::ostream &out) const
{
    out << "Control instance: " << control << std::endl;
}

void ompl::control::ControlManifold::printSettings(std::ostream &out) const
{
    out << "ControlManifold '" << getName() << "' instance: " << this << std::endl;
}

void ompl::control::ControlManifold::propagate(const base::State *state, const Control* control, const double duration, base::State *result) const
{
    if (statePropagation_)
        statePropagation_(state, control, duration, result);
    else
        throw Exception("State propagation routine is not set for control manifold. Either set this routine or provide a different implementation in an inherited class.");
}

void ompl::control::ControlManifold::setPropagationFunction(const StatePropagationFn &fn)
{
    statePropagation_ = fn;
}

void ompl::control::CompoundControlManifold::addSubManifold(const ControlManifoldPtr &component)
{
    if (locked_)
        throw Exception("This manifold is locked. No further components can be added");

    components_.push_back(component);
    componentCount_ = components_.size();
}

unsigned int ompl::control::CompoundControlManifold::getSubManifoldCount(void) const
{
    return componentCount_;
}

const ompl::control::ControlManifoldPtr& ompl::control::CompoundControlManifold::getSubManifold(const unsigned int index) const
{
    if (componentCount_ > index)
        return components_[index];
    else
        throw Exception("Submanifold index does not exist");
}

const ompl::control::ControlManifoldPtr& ompl::control::CompoundControlManifold::getSubManifold(const std::string &name) const
{
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        if (components_[i]->getName() == name)
            return components_[i];
    throw Exception("Submanifold " + name + " does not exist");
}

unsigned int ompl::control::CompoundControlManifold::getDimension(void) const
{
    unsigned int dim = 0;
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        dim += components_[i]->getDimension();
    return dim;
}

ompl::control::Control* ompl::control::CompoundControlManifold::allocControl(void) const
{
    CompoundControl *control = new CompoundControl();
    control->components = new Control*[componentCount_];
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        control->components[i] = components_[i]->allocControl();
    return static_cast<Control*>(control);
}

void ompl::control::CompoundControlManifold::freeControl(Control *control) const
{
    CompoundControl *ccontrol = static_cast<CompoundControl*>(control);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->freeControl(ccontrol->components[i]);
    delete[] ccontrol->components;
    delete ccontrol;
}

void ompl::control::CompoundControlManifold::copyControl(Control *destination, const Control *source) const
{
    CompoundControl      *cdest = static_cast<CompoundControl*>(destination);
    const CompoundControl *csrc = static_cast<const CompoundControl*>(source);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->copyControl(cdest->components[i], csrc->components[i]);
}

bool ompl::control::CompoundControlManifold::equalControls(const Control *control1, const Control *control2) const
{
    const CompoundControl *ccontrol1 = static_cast<const CompoundControl*>(control1);
    const CompoundControl *ccontrol2 = static_cast<const CompoundControl*>(control2);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        if (!components_[i]->equalControls(ccontrol1->components[i], ccontrol2->components[i]))
            return false;
    return true;
}

void ompl::control::CompoundControlManifold::nullControl(Control *control) const
{
    CompoundControl *ccontrol = static_cast<CompoundControl*>(control);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->nullControl(ccontrol->components[i]);
}

ompl::control::ControlSamplerPtr ompl::control::CompoundControlManifold::allocControlSampler(void) const
{
    CompoundControlSampler *ss = new CompoundControlSampler(this);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        ss->addSampler(components_[i]->allocControlSampler());
    return ControlSamplerPtr(ss);
}

void ompl::control::CompoundControlManifold::propagate(const base::State *state, const Control* control, const double duration, base::State *result) const
{
    if (statePropagation_)
        statePropagation_(state, control, duration, result);
    else
    {
        const base::CompoundState *cstate = static_cast<const base::CompoundState*>(state);
        const CompoundControl *ccontrol = static_cast<const CompoundControl*>(control);
        base::CompoundState *cresult = static_cast<base::CompoundState*>(result);
        for (unsigned int i = 0 ; i < componentCount_ ; ++i)
            components_[i]->propagate(cstate->components[i], ccontrol->components[i], duration, cresult->components[i]);
    }
}

void ompl::control::CompoundControlManifold::lock(void)
{
    locked_ = true;
}

bool ompl::control::CompoundControlManifold::canPropagateBackward(void) const
{
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        if (!components_[i]->canPropagateBackward())
            return false;
    return true;
}

double* ompl::control::CompoundControlManifold::getValueAddressAtIndex(Control *control, const unsigned int index) const
{
    CompoundControl *ccontrol = static_cast<CompoundControl*>(control);
    unsigned int idx = 0;

    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        for (unsigned int j = 0 ; j <= index ; ++j)
        {
            double *va = components_[i]->getValueAddressAtIndex(ccontrol->components[i], j);
            if (va)
            {
                if (idx == index)
                    return va;
                else
                    idx++;
            }
            else
                break;
        }
    return NULL;
}

void ompl::control::CompoundControlManifold::printControl(const Control *control, std::ostream &out) const
{
    out << "Compound control [" << std::endl;
    const CompoundControl *ccontrol = static_cast<const CompoundControl*>(control);
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->printControl(ccontrol->components[i], out);
    out << "]" << std::endl;
}

void ompl::control::CompoundControlManifold::printSettings(std::ostream &out) const
{
    out << "Compound control manifold '" << getName() << "' [" << std::endl;
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->printSettings(out);
    out << "]" << std::endl;
}

void ompl::control::CompoundControlManifold::setup(void)
{
    for (unsigned int i = 0 ; i < componentCount_ ; ++i)
        components_[i]->setup();
    ControlManifold::setup();
}
