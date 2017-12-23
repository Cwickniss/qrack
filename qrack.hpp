//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano 2017. All rights reserved.
//
// This is a header-only, quick-and-dirty, multithreaded, universal quantum register
// simulation, allowing (nonphysical) register cloning and direct measurement of
// probability and phase, to leverage what advantages classical emulation of qubits
// can have.
//
// The greater work, except where noted, is licensed under
// the GNU General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/gpl-3.0.en.html
// for details.

#include <stdint.h>
#include <math.h>
#include <complex>
#include <random>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <future>

#define Complex16 std::complex<double>
#define bitLenInt uint8_t
#define bitCapInt uint64_t
#define bitsInByte 8

#include "par_for.hpp"

namespace Qrack {
	class Register {
		public:
			Register(bitLenInt qBitCount) : rand_distribution(0.0, 1.0) {
				if (qBitCount > (sizeof(bitCapInt) * bitsInByte))
					throw std::invalid_argument("Cannot instantiate a register with greater capacity than native types on emulating system.");
				double angle = rand_distribution(rand_generator) * 2.0 * M_PI;
				double cosine = cos(angle);
				double sine = sin(angle);

				runningNorm = 1.0;
				qubitCount = qBitCount;
				maxQPower = 1<<qBitCount;
				stateVec = new Complex16[maxQPower];
				bitCapInt lcv;
				stateVec[0] = Complex16(cosine, sine);
				for (lcv = 1; lcv < maxQPower; lcv++) {
					stateVec[lcv] = Complex16(0.0, 0.0);
				}
			}
			Register(bitLenInt qBitCount, bitCapInt initState) : rand_distribution(0.0, 1.0) {
				double angle = rand_distribution(rand_generator) * 2.0 * M_PI;
				double cosine = cos(angle);
				double sine = sin(angle);

				runningNorm = 1.0;
				qubitCount = qBitCount;
				maxQPower = 1<<qBitCount;
				stateVec = new Complex16[maxQPower];
				bitCapInt lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					if (lcv == initState) {
						stateVec[lcv] = Complex16(cosine, sine);
					}	
					else {
						stateVec[lcv] = Complex16(0.0, 0.0);
					}
				}
			}
			Register(const Register& pqs) : rand_distribution(0.0, 1.0) {
				runningNorm = pqs.runningNorm;
				qubitCount = pqs.qubitCount;
				maxQPower = pqs.maxQPower;
				stateVec = new Complex16[maxQPower];
				std::copy(pqs.stateVec, pqs.stateVec + qubitCount, stateVec);
			}
			~Register() {
				delete [] stateVec;
			}

			int GetQubitCount() {
				return qubitCount;
			}
			void CloneRawState(Complex16* output) {
				if (runningNorm != 1.0) NormalizeState();
				std::copy(stateVec, stateVec + qubitCount, output);
			}
			double Rand() {
				return rand_distribution(rand_generator);
			}
			void SetPermutation(bitCapInt perm) {
				double angle = rand_distribution(rand_generator) * 2.0 * M_PI;
				double cosine = cos(angle);
				double sine = sin(angle);

				runningNorm = 1.0;
				bitCapInt lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					if (lcv == perm) {
						stateVec[lcv] = Complex16(cosine, sine);
					}	
					else {
						stateVec[lcv] = Complex16(0.0, 0.0);
					}
				}
			}
			void SetQuantumState(Complex16* inputState) {
				std::copy(inputState, inputState + qubitCount, stateVec);
			}

			//Logic Gates:
			void CCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target) {
				//if ((control1 >= qubitCount) || (control2 >= qubitCount))
				//	throw std::invalid_argument("CCNOT tried to operate on bit index greater than total bits.");
				if (control1 == control2) throw std::invalid_argument("CCNOT control bits cannot be same bit.");
				if (control1 == target || control2 == target)
					throw std::invalid_argument("CCNOT control bits cannot also be target.");

				const Complex16 pauliX[4] = {
					Complex16(0.0, 0.0), Complex16(1.0, 0.0),
					Complex16(1.0, 0.0), Complex16(0.0, 0.0)
				};

				bitCapInt qPowers[4];
				qPowers[1] = 1 << control1;
				qPowers[2] = 1 << control2;
				qPowers[3] = 1 << target;
				qPowers[0] = qPowers[1] + qPowers[2] + qPowers[3];
				//Complex16 b = Complex16(0.0, 0.0);
				par_for (0, maxQPower, stateVec, Complex16(1.0 / runningNorm, 0.0), pauliX, qPowers,
					[](const int lcv, const int cpu, Complex16* stateVec, const Complex16 nrm, const Complex16* mtrx, const bitCapInt* qPowers) {
						if ((lcv & qPowers[0]) == 0) {
							Complex16 qubit[2];

							qubit[0] = stateVec[lcv + qPowers[1] + qPowers[2] + qPowers[3]];
							qubit[1] = stateVec[lcv + qPowers[1] + qPowers[2]];						

							Complex16 Y0 = qubit[0];
							qubit[0] = nrm * (mtrx[0] * Y0 + mtrx[1] * qubit[1]);
							qubit[1] = nrm * (mtrx[2] * Y0 + mtrx[3] * qubit[1]);

							stateVec[lcv + qPowers[1] + qPowers[2] + qPowers[3]] = qubit[0];
							stateVec[lcv + qPowers[1] + qPowers[2]] = qubit[1];
						}
					}
				);

				UpdateRunningNorm();
			}
			void CNOT(bitLenInt control, bitLenInt target) {
				//if ((control >= qubitCount) || (target >= qubitCount))
				//	throw std::invalid_argument("CNOT tried to operate on bit index greater than total bits.");
				if (control == target) throw std::invalid_argument("CNOT control bit cannot also be target.");

				const Complex16 pauliX[4] = {
					Complex16(0.0, 0.0), Complex16(1.0, 0.0),
					Complex16(1.0, 0.0), Complex16(0.0, 0.0)
				};
				ApplyControlled2x2(control, target, pauliX);
			}
			void H(bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("H tried to operate on bit index greater than total bits.");
				if (runningNorm != 1.0) NormalizeState();

				const Complex16 had[4] = {
					Complex16(1.0 / M_SQRT2, 0.0), Complex16(1.0 / M_SQRT2, 0.0),
					Complex16(1.0 / M_SQRT2, 0.0), Complex16(-1.0 / M_SQRT2, 0.0)
				};
				Apply2x2(qubitIndex, had);
			}
			bool M(bitLenInt qubitIndex) {
				if (runningNorm != 1.0) NormalizeState();

				bool result;
				double prob = rand_distribution(rand_generator);
				double angle = rand_distribution(rand_generator) * 2.0 * M_PI;
				double cosine = cos(angle);
				double sine = sin(angle);

				bitCapInt qPower = 1 << qubitIndex;
				double zeroChance = 0;
				bitCapInt lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					if (lcv & qPower == 0) {
						zeroChance += normSqrd(stateVec + lcv);
					} 
				}

				result = (prob >= zeroChance);
				double nrmlzr = 1.0;
				runningNorm = 0.0;
				
				if (result) {
					if (zeroChance < 1.0) nrmlzr = sqrt(1.0 - zeroChance);
					for (lcv = 0; lcv < maxQPower; lcv++) {
						if ((lcv & qPower) == 0) {
							stateVec[lcv] = Complex16(0.0, 0.0);
						}
						else {
							stateVec[lcv] = Complex16(
								cosine * real(stateVec[lcv]) - sine * imag(stateVec[lcv]),
								sine * real(stateVec[lcv]) + cosine * imag(stateVec[lcv])
							) / nrmlzr;
							runningNorm += normSqrd(stateVec + lcv);
						}
					}
				}
				else {
					if (zeroChance > 0.0) nrmlzr = sqrt(zeroChance);
					for (lcv = 0; lcv < maxQPower; lcv++) {
						if ((lcv & qPower) == 0) {
							stateVec[lcv] = Complex16(
								cosine * real(stateVec[lcv]) - sine * imag(stateVec[lcv]),
								sine * real(stateVec[lcv]) + cosine * imag(stateVec[lcv])
							) / nrmlzr;
							runningNorm += normSqrd(stateVec + lcv);
						}
						else {
							stateVec[lcv] = Complex16(0.0, 0.0);
						}
					}
				}

				runningNorm = sqrt(runningNorm);
			}
			bool MAll(bitCapInt fullRegister) {
				bool result;
				double prob = rand_distribution(rand_generator);
				double angle = rand_distribution(rand_generator) * 2.0 * M_PI;
				double cosine = cos(angle);
				double sine = sin(angle);

				Complex16 toTest = stateVec[fullRegister];
				double oneChance = real(toTest) * real(toTest) + imag(toTest) * imag(toTest);
				result = (prob < oneChance);

				double nrmlzr;
				bitCapInt lcv;
				bitCapInt maxPower = 1 << qubitCount;
				if (result) {
					for (lcv = 0; lcv < maxPower; lcv++) {
						if (lcv == fullRegister) {
							stateVec[lcv] = Complex16(cosine, sine);
						}
						else {
							stateVec[lcv] = Complex16(0.0, 0.0);
						}
					}
				}
				else {
					nrmlzr = sqrt(1.0 - oneChance);
					for (lcv = 0; lcv < maxPower; lcv++) {
						if (lcv == fullRegister) {
							stateVec[lcv] = Complex16(0.0, 0.0);
						}
						else {
							stateVec[lcv] = Complex16(
								cosine * real(stateVec[lcv]) - sine * imag(stateVec[lcv]),
								sine * real(stateVec[lcv]) + cosine * imag(stateVec[lcv])
							) / nrmlzr;
						}
					}
				}

				return result;
			}
			double Prob(bitLenInt qubitIndex) {
				if (runningNorm != 1.0) NormalizeState();

				bitCapInt qPower = 1 << qubitIndex;
				double oneChance = 0;
				bitCapInt lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					if ((lcv & qPower) == qPower) {
						oneChance += normSqrd(stateVec + lcv);
					} 
				}

				return oneChance;
			}
			double ProbAll(bitCapInt fullRegister) {
				if (runningNorm != 1.0) NormalizeState();

				return normSqrd(stateVec + fullRegister);
			}
			void ProbArray(double* probArray) {
				if (runningNorm != 1.0) NormalizeState();

				bitCapInt lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					probArray[lcv] = normSqrd(stateVec + lcv); 
				}
			}
			void R1(double radians, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				double cosine = cos(radians);
				double sine = sin(radians); 
				const Complex16 mtrx[4] = {
					Complex16(1.0, 0), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(cosine, sine)
				};
				Apply2x2(qubitIndex, mtrx);
			}
			void R1Dyad(int numerator, int denominator, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				R1((M_PI * numerator) / denominator, qubitIndex);
			}
			void RX(double radians, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("X tried to operate on bit index greater than total bits.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				Complex16 pauliRX[4] = {
					Complex16(cosine, 0.0), Complex16(0.0, -sine),
					Complex16(0.0, -sine), Complex16(cosine, 0.0)
				};
				Apply2x2(qubitIndex, pauliRX);
			}
			void RXDyad(int numerator, int denominator, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				RX((-M_PI * numerator) / denominator, qubitIndex);
			}
			void RY(double radians, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Y tried to operate on bit index greater than total bits.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				Complex16 pauliRY[4] = {
					Complex16(cosine, 0.0), Complex16(-sine, 0.0),
					Complex16(sine, 0.0), Complex16(cosine, 0.0)
				};
				Apply2x2(qubitIndex, pauliRY);
			}
			void RYDyad(int numerator, int denominator, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				RY((-M_PI * numerator) / denominator, qubitIndex);
			}
			void RZ(double radians, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				const Complex16 pauliRZ[4] = {
					Complex16(cosine, -sine), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(cosine, sine)
				};
				Apply2x2(qubitIndex, pauliRZ);
			}
			void RZDyad(int numerator, int denominator, bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				RZ((-M_PI * numerator) / denominator, qubitIndex);
			}
			void SetBit(bitLenInt qubitIndex1, bool value) {
				if (value != M(qubitIndex1)) {
					X(qubitIndex1);
				}
			}
			void Swap(bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if ((qubitIndex1 >= qubitCount) || (qubitIndex2 >= qubitCount))
				//	throw std::invalid_argument("CNOT tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("Swap bits cannot be the same bit.");
				const Complex16 pauliX[4] = {
					Complex16(0.0, 0.0), Complex16(1.0, 0.0),
					Complex16(1.0, 0.0), Complex16(0.0, 0.0)
				};

				bitCapInt qPowers[3];
				qPowers[1] = 1 << qubitIndex1;
				qPowers[2] = 1 << qubitIndex2;
				qPowers[0] = qPowers[1] + qPowers[2];
				//Complex16 b = Complex16(0.0, 0.0);
				par_for (0, maxQPower, stateVec, Complex16(1.0 / runningNorm, 0.0), pauliX, qPowers,
					[](const int lcv, const int cpu, Complex16* stateVec, const Complex16 nrm, const Complex16* mtrx, const bitCapInt* qPowers) {
						if ((lcv & qPowers[0]) == 0) {
							Complex16 qubit[2];

							qubit[0] = stateVec[lcv + qPowers[2]];
							qubit[1] = stateVec[lcv + qPowers[1]];						

							Complex16 Y0 = qubit[0];
							qubit[0] = nrm * (mtrx[0] * Y0 + mtrx[1] * qubit[1]);
							qubit[1] = nrm * (mtrx[2] * Y0 + mtrx[3] * qubit[1]);

							stateVec[lcv + qPowers[2]] = qubit[0];
							stateVec[lcv + qPowers[1]] = qubit[1];
						}
					}
				);

				UpdateRunningNorm();
			}
			void X(bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("X tried to operate on bit index greater than total bits.");
				const Complex16 pauliX[4] = {
					Complex16(0.0, 0.0), Complex16(1.0, 0.0),
					Complex16(1.0, 0.0), Complex16(0.0, 0.0)
				};
				Apply2x2(qubitIndex, pauliX);
			}
			void XAll() {
				bitLenInt lcv;
				for (lcv = 0; lcv < qubitCount; lcv++) {
					X(lcv);
				}

				UpdateRunningNorm();
			}
			void Y(bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Y tried to operate on bit index greater than total bits.");
				const Complex16 pauliY[4] = {
					Complex16(0.0, 0.0), Complex16(0.0, -1.0),
					Complex16(0.0, 1.0), Complex16(0.0, 0.0)
				};
				Apply2x2(qubitIndex, pauliY);
			}
			void Z(bitLenInt qubitIndex) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				const Complex16 pauliZ[4] = {
					Complex16(1.0, 0.0), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(-1.0, 0.0)
				};
				Apply2x2(qubitIndex, pauliZ);
			}
			void CR1(double radians, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if ((qubitIndex1 >= qubitCount) || (qubitIndex2 >= qubitCount))
				//	throw std::invalid_argument("CNOT tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CR1 control bit cannot also be target.");
				double cosine = cos(radians);
				double sine = sin(radians); 
				const Complex16 mtrx[4] = {
					Complex16(1.0, 0), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(cosine, sine)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, mtrx);
			}
			void CR1Dyad(int numerator, int denominator, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CR1Dyad control bit cannot also be target.");
				CR1((-M_PI * numerator) / denominator, qubitIndex1, qubitIndex2);
			}
			void CRX(double radians, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("X tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRX control bit cannot also be target.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				Complex16 pauliRX[4] = {
					Complex16(cosine, 0.0), Complex16(0.0, -sine),
					Complex16(0.0, -sine), Complex16(cosine, 0.0)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, pauliRX);
			}
			void CRXDyad(int numerator, int denominator, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRXDyad control bit cannot also be target.");
				CRX((-M_PI * numerator) / denominator, qubitIndex1, qubitIndex2);
			}
			void CRY(double radians, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Y tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRY control bit cannot also be target.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				Complex16 pauliRY[4] = {
					Complex16(cosine, 0.0), Complex16(-sine, 0.0),
					Complex16(sine, 0.0), Complex16(cosine, 0.0)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, pauliRY);
			}
			void CRYDyad(int numerator, int denominator, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRYDyad control bit cannot also be target.");
				CRY((-M_PI * numerator) / denominator, qubitIndex1, qubitIndex2);
			}
			void CRZ(double radians, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRZ control bit cannot also be target.");
				double cosine = cos(radians / 2.0);
				double sine = sin(radians / 2.0); 
				const Complex16 pauliRZ[4] = {
					Complex16(cosine, -sine), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(cosine, sine)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, pauliRZ);
			}
			void CRZDyad(int numerator, int denominator, bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CRZDyad control bit cannot also be target.");
				CRZ((-M_PI * numerator) / denominator, qubitIndex1, qubitIndex2);
			}
			void CY(bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Y tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CY control bit cannot also be target.");
				const Complex16 pauliY[4] = {
					Complex16(0.0, 0.0), Complex16(0.0, -1.0),
					Complex16(0.0, 1.0), Complex16(0.0, 0.0)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, pauliY);
			}
			void CZ(bitLenInt qubitIndex1, bitLenInt qubitIndex2) {
				//if (qubitIndex >= qubitCount) throw std::invalid_argument("Z tried to operate on bit index greater than total bits.");
				if (qubitIndex1 == qubitIndex2) throw std::invalid_argument("CZ control bit cannot also be target.");
				const Complex16 pauliZ[4] = {
					Complex16(1.0, 0.0), Complex16(0.0, 0.0),
					Complex16(0.0, 0.0), Complex16(-1.0, 0.0)
				};
				ApplyControlled2x2(qubitIndex1, qubitIndex2, pauliZ);
			}

			//Single register instructions:
			//void LSL() {
			//	ROL();
			//	SetBit(0, false);
			//}
			//void LSR() {
			//	ROR();
			//	SetBit(qubitCount - 1, false);
			//}
			void ROL() {
				int i;
				for (i = 1; i < qubitCount; i++) {
					Swap(i, 0);
				}
			}
			void ROR() {
				int i;
				for (i = qubitCount - 2; i >= 0; i--) {
					Swap(i, qubitCount - 1);
				}
			}
			void QFT() {
				int i, j;
				for (i = 0; i < qubitCount; i++) {
					H(i);
					for (j = 1; j < qubitCount - i; j++) {
						CR1Dyad(1, 1<<j, i + j, i); 
					}
				}
			}


		private:
			double runningNorm;
			bitLenInt qubitCount;
			bitCapInt maxQPower;
			Complex16* stateVec;

			std::default_random_engine rand_generator;
			std::uniform_real_distribution<double> rand_distribution;

			void Apply2x2(bitLenInt qubitIndex, const Complex16* mtrx) {
				bitCapInt qPowers[1];
				qPowers[0] = 1 << qubitIndex;
				//Complex16 b = Complex16(0.0, 0.0);
				par_for (0, maxQPower, stateVec, Complex16(1.0 / runningNorm, 0.0), mtrx, qPowers,
					[](const int lcv, const int cpu, Complex16* stateVec, const Complex16 nrm, const Complex16* mtrx, const bitCapInt* qPowers) {
						if ((lcv & qPowers[0]) == 0) {
							Complex16 qubit[2];

							qubit[0] = stateVec[lcv + qPowers[0]];
							qubit[1] = stateVec[lcv];						

							Complex16 Y0 = qubit[0];
							qubit[0] = nrm * (mtrx[0] * Y0 + mtrx[1] * qubit[1]);
							qubit[1] = nrm * (mtrx[2] * Y0 + mtrx[3] * qubit[1]);

							stateVec[lcv + qPowers[0]] = qubit[0];
							stateVec[lcv] = qubit[1];
						}
					}
				);

				UpdateRunningNorm();
			}

			void ApplyControlled2x2(bitLenInt qubitIndex1, bitLenInt qubitIndex2, const Complex16* mtrx) {
				bitCapInt qPowers[3];
				qPowers[1] = 1 << qubitIndex1;
				qPowers[2] = 1 << qubitIndex2;
				qPowers[0] = qPowers[1] + qPowers[2];
				//Complex16 b = Complex16(0.0, 0.0);
				par_for (0, maxQPower, stateVec, Complex16(1.0 / runningNorm, 0.0), mtrx, qPowers,
					[](const int lcv, const int cpu, Complex16* stateVec, const Complex16 nrm, const Complex16* mtrx, const bitCapInt* qPowers) {
						if ((lcv & qPowers[0]) == 0) {
							Complex16 qubit[2];

							qubit[0] = stateVec[lcv + qPowers[1] + qPowers[2]];
							qubit[1] = stateVec[lcv + qPowers[1]];						

							Complex16 Y0 = qubit[0];
							qubit[0] = nrm * (mtrx[0] * Y0 + mtrx[1] * qubit[1]);
							qubit[1] = nrm * (mtrx[2] * Y0 + mtrx[3] * qubit[1]);

							stateVec[lcv + qPowers[1] + qPowers[2]] = qubit[0];
							stateVec[lcv + qPowers[1]] = qubit[1];
						}
					}
				);

				UpdateRunningNorm();
			}

			void UpdateRunningNorm() {
				long int lcv;
				double sqrNorm = 0.0;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					sqrNorm += normSqrd(stateVec + lcv);
				}

				runningNorm = sqrt(sqrNorm);
			}

			void NormalizeState() {
				long int lcv;
				for (lcv = 0; lcv < maxQPower; lcv++) {
					stateVec[lcv] /= runningNorm;
				}
				runningNorm = 1.0;
			}

			double normSqrd(Complex16* cmplx) {
				return real(*cmplx) * real(*cmplx) + imag(*cmplx) * imag(*cmplx);
			}
	};
}
