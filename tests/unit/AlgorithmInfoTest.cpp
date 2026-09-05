#include <gtest/gtest.h>
#include "dsp/AlgorithmInfo.h"

// AlgorithmInfo must agree with ymfm's algorithm encoding. The reference values
// below are ymfm's s_algorithm_ops entries for OPM algorithms 0-7:
//   bits 0     : opout index used as operator 2 input
//   bits 1-3   : opout index used as operator 3 input
//   bits 4-6   : opout index used as operator 4 input
//   bits 7,8,9 : operators 1, 2, 3 included in the final sum (4 always is)
// where opout[1..3] are operators 1..3, opout[5] = 1+2, opout[6] = 1+3, opout[7] = 2+3.
namespace {

constexpr uint16_t ALG(int op2in, int op3in, int op4in, int op1out, int op2out, int op3out)
{
    return static_cast<uint16_t>(op2in | (op3in << 1) | (op4in << 4) | (op1out << 7) | (op2out << 8) | (op3out << 9));
}

constexpr uint16_t kYmfmOps[8] = {
    ALG(1,2,3, 0,0,0), ALG(0,5,3, 0,0,0), ALG(0,2,6, 0,0,0), ALG(1,0,7, 0,0,0),
    ALG(1,0,3, 0,1,0), ALG(1,1,1, 0,1,1), ALG(1,0,0, 0,1,1), ALG(0,0,0, 1,1,1),
};

// Which operators (0-based) are summed into opout[index]
bool opoutContains(int index, int op)
{
    switch (index) {
        case 1: return op == 0;
        case 2: return op == 1;
        case 3: return op == 2;
        case 5: return op == 0 || op == 1;
        case 6: return op == 0 || op == 2;
        case 7: return op == 1 || op == 2;
        default: return false;
    }
}

} // namespace

TEST(AlgorithmInfoTest, EdgesMatchYmfmEncoding)
{
    for (int alg = 0; alg < 8; ++alg) {
        SCOPED_TRACE("algorithm " + std::to_string(alg));
        const auto& info = ymulatorsynth::kAlgorithms[static_cast<size_t>(alg)];
        const uint16_t ops = kYmfmOps[alg];
        const int inputs[4] = { 0, ops & 1, (ops >> 1) & 7, (ops >> 4) & 7 };
        for (int from = 0; from < 4; ++from)
            for (int to = 0; to < 4; ++to)
                EXPECT_EQ(info.modulates(from, to), opoutContains(inputs[to], from))
                    << "edge " << from + 1 << " -> " << to + 1;
    }
}

TEST(AlgorithmInfoTest, CarriersMatchYmfmOutputBits)
{
    for (int alg = 0; alg < 8; ++alg) {
        SCOPED_TRACE("algorithm " + std::to_string(alg));
        const auto& info = ymulatorsynth::kAlgorithms[static_cast<size_t>(alg)];
        const uint16_t ops = kYmfmOps[alg];
        const bool expected[4] = { ((ops >> 7) & 1) != 0, ((ops >> 8) & 1) != 0, ((ops >> 9) & 1) != 0, true };
        for (int op = 0; op < 4; ++op)
            EXPECT_EQ(info.isCarrier(op), expected[op]) << "operator " << op + 1;
        EXPECT_EQ(info.modulatorMask(), static_cast<uint8_t>(~info.carrierMask & 0x0F));
    }
}

TEST(AlgorithmInfoTest, EveryModulatorHasExactlyOneTargetAndCarriersHaveNone)
{
    for (int alg = 0; alg < 8; ++alg) {
        const auto& info = ymulatorsynth::kAlgorithms[static_cast<size_t>(alg)];
        for (int op = 0; op < 4; ++op) {
            int targets = 0;
            for (int to = 0; to < 4; ++to) if (info.modulates(op, to)) ++targets;
            if (info.isCarrier(op)) EXPECT_EQ(targets, 0) << "alg " << alg << " op " << op + 1;
            else EXPECT_GE(targets, 1) << "alg " << alg << " op " << op + 1;
            EXPECT_EQ(info.targetOf(op) >= 0, !info.isCarrier(op));
        }
        EXPECT_NE(info.structure, nullptr);
        EXPECT_NE(info.description, nullptr);
    }
}
