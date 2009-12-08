/* 
 * Copyright 2009 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/* 
 * File:   pair.h
 * Author: ruehle
 *
 * Created on March 5, 2009, 10:30 AM
 */

#ifndef _BEADPAIR_H
#define	_BEADPAIR_H

/**
   \brief A particle pair
 
   This class defines a particle pair. The future plan is, that the Pair class
   can be overloaded and Particle list creates these inherited pairs.
 
 */

class BeadPair
    : public std::pair<Bead *, Bead *>
{
public:
    BeadPair() {}
    BeadPair(Bead *bead1, Bead *bead2, vec r)
            : std::pair<Bead *, Bead *>(bead1, bead2), _r(r), _dist(abs(r)) {}
        
    /// \brief the vector connecting two beads
    vec &r() { return _r; }
    /// \brief the distance of the beads
    double &dist() { return _dist; }
    
private:
        vec _r;
        double _dist;
};

#endif	/* _PAIR_H */

