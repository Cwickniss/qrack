////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// THE FOLLOWING SECTION IS ADAPTED FROM THE "GILGAMESH" PROJECT, ( https://github.com/andy-thomason/gilgamesh ), UNDER THE MIT LICENSE.
// THE MIT LICENSE APPLIES ONLY TO THIS EXCERPT, IN ACCORDANCE WITH THAT LICENSE.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Copyright (c) 2016 Andy Thomason
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the  
//Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
//and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
//PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
//CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
namespace Qrack {
	template <class F>
	void par_for(const bitCapInt begin, const bitCapInt end, Complex16* stateArray, const Complex16 nrm, const Complex16* mtrx, const bitCapInt* maskArray, F fn) {
		std::atomic<bitCapInt> idx;
		idx = begin;
		int num_cpus = std::thread::hardware_concurrency();
		std::vector<std::future<void>> futures(num_cpus);
		for (int cpu = 0; cpu != num_cpus; ++cpu) {
			futures[cpu] = std::async(std::launch::async, [cpu, &idx, end, stateArray, nrm, mtrx, maskArray, &fn]() {
				for (;;) {
					bitCapInt i = idx++;
					if (i >= end) break;
					fn(i, cpu, stateArray, nrm, mtrx, maskArray);
				}
			});
		}

		for (int cpu = 0; cpu != num_cpus; ++cpu) {
			futures[cpu].get();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// THIS ENDS THE EXCERPTED SECTION FROM "GILGAMESH."
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
