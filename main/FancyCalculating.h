/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 02.02.2024

  Copyright (C) 2024-now Authors and www.dsp-crowd.com

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FANCY_CALCULATING_H
#define FANCY_CALCULATING_H

#include "Processing.h"

class FancyCalculating : public Processing
{

public:

	static FancyCalculating *create()
	{
		return new (std::nothrow) FancyCalculating;
	}

	void paramSet(uint32_t timeCalcMs, uint32_t cntCalcMax);

protected:

	FancyCalculating();
	virtual ~FancyCalculating() {}

private:

	FancyCalculating(const FancyCalculating &) : Processing("") {}
	FancyCalculating &operator=(const FancyCalculating &) { return *this; }

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	uint32_t mStartMs;
	uint32_t mTimeCalcMs;
	uint32_t mCntCalc;
	uint32_t mCntCalcMax;

	/* static functions */

	/* static variables */

	/* constants */

};

#endif

