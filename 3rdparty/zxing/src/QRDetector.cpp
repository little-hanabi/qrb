/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRDetector.h"

#include "BitArray.h"
#include "BitMatrix.h"
#include "BitMatrixCursor.h"
#include "ConcentricFinder.h"
#include "GridSampler.h"
#include "Pattern.h"
#include "QRVersion.h"
#include "Quadrilateral.h"
#include "RegressionLine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

namespace ZXing::QRCode {

constexpr auto PATTERN = FixedPattern<5, 7>{1, 1, 3, 1, 1};
constexpr bool E2E = true;

PatternView FindPattern(const PatternView& view)
{
	return FindLeftGuard<PATTERN.size()>(view, PATTERN.size(), [](const PatternView& view, int spaceInPixel) {
		// perform a fast plausability test for 1:1:3:1:1 pattern
		if (view[2] < 2 * std::max(view[0], view[4]) || view[2] < std::max(view[1], view[3]))
			return 0.;
		return IsPattern<E2E>(view, PATTERN, spaceInPixel, 0.1); // the requires 4, here we accept almost 0
	});
}

std::vector<ConcentricPattern> FindFinderPatterns(const BitMatrix& image, bool tryHarder)
{
	constexpr int MIN_SKIP         = 3;           // 1 pixel/module times 3 modules/center
	constexpr int MAX_MODULES_FAST = 20 * 4 + 17; // support up to version 20 for mobile clients

	// Let's assume that the maximum version QR Code we support takes up 1/4 the height of the
	// image, and then account for the center being 3 modules in size. This gives the smallest
	// number of pixels the center could be, so skip this often. When trying harder, look for all
	// QR versions regardless of how dense they are.
	int height = image.height();
	int skip = (3 * height) / (4 * MAX_MODULES_FAST);
	if (skip < MIN_SKIP || tryHarder)
		skip = MIN_SKIP;

	std::vector<ConcentricPattern> res;
	[[maybe_unused]] int N = 0;
	PatternRow row;

	for (int y = skip - 1; y < height; y += skip) {
		GetPatternRow(image, y, row, false);
		PatternView next = row;

		while (next = FindPattern(next), next.isValid()) {
			PointF p(next.pixelsInFront() + next[0] + next[1] + next[2] / 2.0, y + 0.5);

			// make sure p is not 'inside' an already found pattern area
			if (FindIf(res, [p](const auto& old) { return distance(p, old) < old.size / 2; }) == res.end()) {
				N++;
				auto pattern = LocateConcentricPattern<E2E>(image, PATTERN, p,
															next.sum() * 3); // 3 for very skewed samples
				if (pattern) {
					assert(image.get(pattern->x, pattern->y));
					res.push_back(*pattern);
				}
			}

			next.skipPair();
			next.skipPair();
			next.extend();
		}
	}

	return res;
}

/**
 * @brief GenerateFinderPatternSets
 * @param patterns list of ConcentricPattern objects, i.e. found finder pattern squares
 * @return list of plausible finder pattern sets, sorted by decreasing plausibility
 */
FinderPatternSets GenerateFinderPatternSets(FinderPatterns& patterns)
{
	std::sort(patterns.begin(), patterns.end(), [](const auto& a, const auto& b) { return a.size < b.size; });

	auto sets            = std::multimap<double, FinderPatternSet>();
	auto squaredDistance = [](const auto* a, const auto* b) {
		// The scaling of the distance by the b/a size ratio is a very coarse compensation for the shortening effect of
		// the camera projection on slanted symbols. The fact that the size of the finder pattern is proportional to the
		// distance from the camera is used here. This approximation only works if a < b < 2*a (see below).
		// Test image: fix-finderpattern-order.jpg
		return dot((*a - *b), (*a - *b)) * std::pow(double(b->size) / a->size, 2);
	};
	const double cosUpper = std::cos(45. / 180 * 3.1415); // TODO: use c++20 std::numbers::pi_v
	const double cosLower = std::cos(135. / 180 * 3.1415);

	int nbPatterns = Size(patterns);
	for (int i = 0; i < nbPatterns - 2; i++) {
		for (int j = i + 1; j < nbPatterns - 1; j++) {
			for (int k = j + 1; k < nbPatterns - 0; k++) {
				const auto* a = &patterns[i];
				const auto* b = &patterns[j];
				const auto* c = &patterns[k];
				// if the pattern sizes are too different to be part of the same symbol, skip this
				// and the rest of the innermost loop (sorted list)
				if (c->size > a->size * 2)
					break;

				// Orders the three points in an order [A,B,C] such that AB is less than AC
				// and BC is less than AC, and the angle between BC and BA is less than 180 degrees.

				auto distAB2 = squaredDistance(a, b);
				auto distBC2 = squaredDistance(b, c);
				auto distAC2 = squaredDistance(a, c);

				if (distBC2 >= distAB2 && distBC2 >= distAC2) {
					std::swap(a, b);
					std::swap(distBC2, distAC2);
				} else if (distAB2 >= distAC2 && distAB2 >= distBC2) {
					std::swap(b, c);
					std::swap(distAB2, distAC2);
				}

				auto distAB = std::sqrt(distAB2);
				auto distBC = std::sqrt(distBC2);

				// Make sure distAB and distBC don't differ more than reasonable
				// TODO: make sure the constant 2 is not to conservative for reasonably tilted symbols
				if (distAB > 2 * distBC || distBC > 2 * distAB)
					continue;

				// Estimate the module count and ignore this set if it can not result in a valid decoding
				if (auto moduleCount = (distAB + distBC) / (2 * (a->size + b->size + c->size) / (3 * 7.f)) + 7;
					moduleCount < 21 * 0.9 || moduleCount > 177 * 1.5) // moduleCount may be overestimated, see above
					continue;

				// Make sure the angle between AB and BC does not deviate from 90° by more than 45°
				auto cosAB_BC = (distAB2 + distBC2 - distAC2) / (2 * distAB * distBC);
				if (std::isnan(cosAB_BC) || cosAB_BC > cosUpper || cosAB_BC < cosLower)
					continue;

				// a^2 + b^2 = c^2 (Pythagorean theorem), and a = b (isosceles triangle).
				// Since any right triangle satisfies the formula c^2 - b^2 - a^2 = 0,
				// we need to check both two equal sides separately.
				// The value of |c^2 - 2 * b^2| + |c^2 - 2 * a^2| increases as dissimilarity
				// from isosceles right triangle.
				double d = (std::abs(distAC2 - 2 * distAB2) + std::abs(distAC2 - 2 * distBC2));

				// Use cross product to figure out whether A and C are correct or flipped.
				// This asks whether BC x BA has a positive z component, which is the arrangement
				// we want for A, B, C. If it's negative then swap A and C.
				if (cross(*c - *b, *a - *b) < 0)
					std::swap(a, c);

				// arbitrarily limit the number of potential sets
				// (this has performance implications while limiting the maximal number of detected symbols)
				const auto setSizeLimit = 256;
				if (sets.size() < setSizeLimit || sets.crbegin()->first > d) {
					sets.emplace(d, FinderPatternSet{*a, *b, *c});
					if (sets.size() > setSizeLimit)
						sets.erase(std::prev(sets.end()));
				}
			}
		}
	}

	// convert from multimap to vector
	FinderPatternSets res;
	res.reserve(sets.size());
	for (auto& [d, s] : sets)
		res.push_back(s);

	return res;
}

static double EstimateModuleSize(const BitMatrix& image, ConcentricPattern a, ConcentricPattern b)
{
	BitMatrixCursorF cur(image, a, b - a);
	assert(cur.isBlack());

	auto pattern = ReadSymmetricPattern<5>(cur, a.size * 2);
	if (!pattern || !IsPattern<true>(*pattern, PATTERN))
		return -1;

	return (2 * Reduce(*pattern) - (*pattern)[0] - (*pattern)[4]) / 12.0 * length(cur.d);
}

struct DimensionEstimate
{
	int dim = 0;
	double ms = 0;
	int err = 4;
};

static DimensionEstimate EstimateDimension(const BitMatrix& image, ConcentricPattern a, ConcentricPattern b)
{
	auto ms_a = EstimateModuleSize(image, a, b);
	auto ms_b = EstimateModuleSize(image, b, a);

	if (ms_a < 0 || ms_b < 0)
		return {};

	auto moduleSize = (ms_a + ms_b) / 2;

	int dimension = narrow_cast<int>(std::lround(distance(a, b) / moduleSize) + 7);
	int error     = 1 - (dimension % 4);

	return {dimension + error, moduleSize, std::abs(error)};
}

static RegressionLine TraceLine(const BitMatrix& image, PointF p, PointF d, int edge)
{
	BitMatrixCursorF cur(image, p, d - p);
	RegressionLine line;
	line.setDirectionInward(cur.back());

	// collect points inside the black line -> backup on 3rd edge
	cur.stepToEdge(edge, 0, edge == 3);
	if (edge == 3)
		cur.turnBack();

	auto curI = BitMatrixCursorI(image, PointI(cur.p), PointI(mainDirection(cur.d)));
	// make sure curI positioned such that the white->black edge is directly behind
	// Test image: fix-traceline.jpg
	while (!curI.edgeAtBack()) {
		if (curI.edgeAtLeft())
			curI.turnRight();
		else if (curI.edgeAtRight())
			curI.turnLeft();
		else
			curI.step(-1);
	}

	for (auto dir : {Direction::LEFT, Direction::RIGHT}) {
		auto c = BitMatrixCursorI(image, curI.p, curI.direction(dir));
		auto stepCount = static_cast<int>(maxAbsComponent(cur.p - p));
		do {
			line.add(centered(c.p));
		} while (--stepCount > 0 && c.stepAlongEdge(dir, true));
	}

	line.evaluate(1.0, true);

	return line;
}

// estimate how tilted the symbol is (return value between 1 and 2, see also above)
static double EstimateTilt(const FinderPatternSet& fp)
{
	int min = std::min({fp.bl.size, fp.tl.size, fp.tr.size});
	int max = std::max({fp.bl.size, fp.tl.size, fp.tr.size});
	return double(max) / min;
}

static PerspectiveTransform Mod2Pix(int dimension, PointF brOffset, QuadrilateralF pix)
{
	auto quad = Rectangle(dimension, dimension, 3.5);
	quad[2] = quad[2] - brOffset;
	return {quad, pix};
}

static std::optional<PointF> LocateAlignmentPattern(const BitMatrix& image, int moduleSize, PointF estimate)
{
	for (auto d : {PointF{0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1},
#if 1
				   }) {
#else
				   {0, -2}, {0, 2}, {-2, 0}, {2, 0}, {-1, -2}, {1, -2}, {-1, 2}, {1, 2}, {-2, -1}, {-2, 1}, {2, -1}, {2, 1}}) {
#endif
		auto cor = CenterOfRing(image, PointI(estimate + moduleSize * 2.25 * d), moduleSize * 3, 1, false);

		// if we did not land on a black pixel the concentric pattern finder will fail
		if (!cor || !image.get(*cor))
			continue;

		if (auto cor1 = CenterOfRing(image, PointI(*cor), moduleSize, 1))
			if (auto cor2 = CenterOfRing(image, PointI(*cor), moduleSize * 3, -2))
				if (distance(*cor1, *cor2) < moduleSize / 2) {
					auto res = (*cor1 + *cor2) / 2;
					return res;
				}
	}

	return {};
}

static const Version* ReadVersion(const BitMatrix& image, int dimension, const PerspectiveTransform& mod2Pix)
{
	int bits[2] = {};

	for (bool mirror : {false, true}) {
		// Read top-right/bottom-left version info: 3 wide by 6 tall (depending on mirrored)
		int versionBits = 0;
		for (int y = 5; y >= 0; --y)
			for (int x = dimension - 9; x >= dimension - 11; --x) {
				auto mod = mirror ? PointI{y, x} : PointI{x, y};
				auto pix = mod2Pix(centered(mod));
				if (!image.isIn(pix))
					versionBits = -1;
				else
					AppendBit(versionBits, image.get(pix));
			}
		bits[static_cast<int>(mirror)] = versionBits;
	}

	return Version::DecodeVersionInformation(bits[0], bits[1]);
}

DetectorResult SampleQR(const BitMatrix& image, const FinderPatternSet& fp)
{
	auto top  = EstimateDimension(image, fp.tl, fp.tr);
	auto left = EstimateDimension(image, fp.tl, fp.bl);

	if (!top.dim && !left.dim)
		return {};

	auto best = top.err == left.err ? (top.dim > left.dim ? top : left) : (top.err < left.err ? top : left);
	int dimension = best.dim;
	int moduleSize = static_cast<int>(best.ms + 1);

	auto br = PointF{-1, -1};
	auto brOffset = PointF{3, 3};

	// Everything except version 1 (21 modules) has an alignment pattern. Estimate the center of that by intersecting
	// line extensions of the 1 module wide square around the finder patterns. This could also help with detecting
	// slanted symbols of version 1.

	// generate 4 lines: outer and inner edge of the 1 module wide black line between the two outer and the inner
	// (tl) finder pattern
	auto bl2 = TraceLine(image, fp.bl, fp.tl, 2);
	auto bl3 = TraceLine(image, fp.bl, fp.tl, 3);
	auto tr2 = TraceLine(image, fp.tr, fp.tl, 2);
	auto tr3 = TraceLine(image, fp.tr, fp.tl, 3);

	if (bl2.isValid() && tr2.isValid() && bl3.isValid() && tr3.isValid()) {
		// intersect both outer and inner line pairs and take the center point between the two intersection points
		auto brInter = (intersect(bl2, tr2) + intersect(bl3, tr3)) / 2;

		if (dimension > 21)
			if (auto brCP = LocateAlignmentPattern(image, moduleSize, brInter))
				br = *brCP;

		// if the symbol is tilted or the resolution of the RegressionLines is sufficient, use their intersection
		// as the best estimate (see discussion in #199 and test image estimate-tilt.jpg )
		if (!image.isIn(br) && (EstimateTilt(fp) > 1.1 || (bl2.isHighRes() && bl3.isHighRes() && tr2.isHighRes() && tr3.isHighRes())))
			br = brInter;
	}

	// otherwise the simple estimation used by upstream is used as a best guess fallback
	if (!image.isIn(br)) {
		br = fp.tr - fp.tl + fp.bl;
		brOffset = PointF(0, 0);
	}

	auto mod2Pix = Mod2Pix(dimension, brOffset, {fp.tl, fp.tr, br, fp.bl});

	if( dimension >= Version::SymbolSize(7, Type::Model2).x) {
		auto version = ReadVersion(image, dimension, mod2Pix);

		// if the version bits are garbage -> discard the detection
		if (!version || std::abs(version->dimension() - dimension) > 8)
			return DetectorResult();
		if (version->dimension() != dimension) {
			dimension = version->dimension();
			mod2Pix = Mod2Pix(dimension, brOffset, {fp.tl, fp.tr, br, fp.bl});
		}
#if 1
		auto& apM = version->alignmentPatternCenters(); // alignment pattern positions in modules
		auto apP = Matrix<std::optional<PointF>>(Size(apM), Size(apM)); // found/guessed alignment pattern positions in pixels
		const int N = Size(apM) - 1;

		// project the alignment pattern at module coordinates x/y to pixel coordinate based on current mod2Pix
		auto projectM2P = [&mod2Pix, &apM](int x, int y) { return mod2Pix(centered(PointI(apM[x], apM[y]))); };

		auto findInnerCornerOfConcentricPattern = [&image, &apP, &projectM2P](int x, int y, const ConcentricPattern& fp) {
			auto pc = *apP.set(x, y, projectM2P(x, y));
			if (auto fpQuad = FindConcentricPatternCorners(image, fp, fp.size, 2))
				for (auto c : *fpQuad)
					if (distance(c, pc) < fp.size / 2)
						apP.set(x, y, c);
		};

		findInnerCornerOfConcentricPattern(0, 0, fp.tl);
		findInnerCornerOfConcentricPattern(0, N, fp.bl);
		findInnerCornerOfConcentricPattern(N, 0, fp.tr);

		auto bestGuessAPP = [&](int x, int y){
			if (auto p = apP(x, y))
				return *p;
			return projectM2P(x, y);
		};

		for (int y = 0; y <= N; ++y)
			for (int x = 0; x <= N; ++x) {
				if (apP(x, y))
					continue;

				PointF guessed =
					x * y == 0 ? bestGuessAPP(x, y) : bestGuessAPP(x - 1, y) + bestGuessAPP(x, y - 1) - bestGuessAPP(x - 1, y - 1);
				if (auto found = LocateAlignmentPattern(image, moduleSize, guessed))
					apP.set(x, y, *found);
			}

		// go over the whole set of alignment patters again and try to fill any remaining gap by using available neighbors as guides
		for (int y = 0; y <= N; ++y)
			for (int x = 0; x <= N; ++x) {
				if (apP(x, y))
					continue;

				// find the two closest valid alignment pattern pixel positions both horizontally and vertically
				std::vector<PointF> hori, verti;
				for (int i = 2; i < 2 * N + 2 && Size(hori) < 2; ++i) {
					int xi = x + i / 2 * (i%2 ? 1 : -1);
					if (0 <= xi && xi <= N && apP(xi, y))
						hori.push_back(*apP(xi, y));
				}
				for (int i = 2; i < 2 * N + 2 && Size(verti) < 2; ++i) {
					int yi = y + i / 2 * (i%2 ? 1 : -1);
					if (0 <= yi && yi <= N && apP(x, yi))
						verti.push_back(*apP(x, yi));
				}

				// if we found 2 each, intersect the two lines that are formed by connecting the point pairs
				if (Size(hori) == 2 && Size(verti) == 2) {
					auto guessed = intersect(RegressionLine(hori[0], hori[1]), RegressionLine(verti[0], verti[1]));
					auto found = LocateAlignmentPattern(image, moduleSize, guessed);
					// search again near that intersection and if the search fails, use the intersection
					apP.set(x, y, found ? *found : guessed);
				}
			}

		if (auto c = apP.get(N, N))
			mod2Pix = Mod2Pix(dimension, PointF(3, 3), {fp.tl, fp.tr, *c, fp.bl});

		// go over the whole set of alignment patters again and fill any remaining gaps by a projection based on an updated mod2Pix
		// projection. This works if the symbol is flat, wich is a reasonable fall-back assumption.
		for (int y = 0; y <= N; ++y)
			for (int x = 0; x <= N; ++x) {
				if (apP(x, y))
					continue;

				apP.set(x, y, projectM2P(x, y));
			}

		// assemble a list of region-of-interests based on the found alignment pattern pixel positions
		ROIs rois;
		for (int y = 0; y < N; ++y)
			for (int x = 0; x < N; ++x) {
				int x0 = apM[x], x1 = apM[x + 1], y0 = apM[y], y1 = apM[y + 1];
				rois.push_back({x0 - (x == 0) * 6, x1 + (x == N - 1) * 7, y0 - (y == 0) * 6, y1 + (y == N - 1) * 7,
								PerspectiveTransform{Rectangle(x0, x1, y0, y1),
													 {*apP(x, y), *apP(x + 1, y), *apP(x + 1, y + 1), *apP(x, y + 1)}}});
			}

		return SampleGrid(image, dimension, dimension, rois);
#endif
	}

	return SampleGrid(image, dimension, dimension, mod2Pix);
}

} // namespace ZXing::QRCode
