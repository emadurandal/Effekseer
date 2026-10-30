#include <Effekseer.h>
#include <Effekseer/Utils/Effekseer.BinaryReader.h>

#include "../TestHelper.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{

template <typename T>
void Append(std::vector<uint8_t>& data, const T& value)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
	data.insert(data.end(), bytes, bytes + sizeof(T));
}

template <typename T>
void Write(std::vector<uint8_t>& data, size_t offset, const T& value)
{
	EXPECT_TRUE(offset + sizeof(T) <= data.size());
	memcpy(data.data() + offset, &value, sizeof(T));
}

std::vector<uint8_t> CreateCurveData()
{
	std::vector<uint8_t> data;
	const int32_t version = Effekseer::Curve::Version;
	const int32_t controlPointCount = 2;
	const int32_t knotCount = 3;
	const int32_t order = 1;
	const int32_t step = 16;
	const int32_t type = 0;
	const int32_t dimension = 3;

	Append(data, version);
	Append(data, controlPointCount);
	Append(data, Effekseer::dVector4(0.0, 0.0, 0.0, 1.0));
	Append(data, Effekseer::dVector4(2.0, 0.0, 0.0, 1.0));
	Append(data, knotCount);
	Append(data, 0.0);
	Append(data, 0.0);
	Append(data, 1.0);
	Append(data, order);
	Append(data, step);
	Append(data, type);
	Append(data, dimension);
	return data;
}

void TestBinaryReaderValidation()
{
	const int32_t expected = 123;
	Effekseer::BinaryReader<true> reader(reinterpret_cast<const uint8_t*>(&expected), sizeof(expected));
	int32_t actual = 0;
	EXPECT_TRUE(reader.Read(actual));
	EXPECT_TRUE(actual == expected);
	EXPECT_TRUE(reader.GetStatus() == Effekseer::BinaryReaderStatus::Complete);
	EXPECT_TRUE(!reader.Read(actual));
	EXPECT_TRUE(reader.GetStatus() == Effekseer::BinaryReaderStatus::Failed);

	Effekseer::BinaryReader<true> overflowReader(reinterpret_cast<const uint8_t*>(&expected), sizeof(expected));
	EXPECT_TRUE(!overflowReader.CanReadElements(-1, sizeof(int32_t)));
	EXPECT_TRUE(!overflowReader.CanReadElements(std::numeric_limits<int32_t>::max(), std::numeric_limits<size_t>::max()));
	EXPECT_TRUE(!overflowReader.Skip(std::numeric_limits<size_t>::max()));
	EXPECT_TRUE(overflowReader.GetStatus() == Effekseer::BinaryReaderStatus::Failed);

	Effekseer::BinaryReader<true> vectorReader(reinterpret_cast<const uint8_t*>(&expected), sizeof(expected));
	std::vector<int32_t> values;
	EXPECT_TRUE(!vectorReader.Read(values, std::numeric_limits<int32_t>::max()));
	EXPECT_TRUE(values.empty());
}

void TestValidCurveData()
{
	auto data = CreateCurveData();
	Effekseer::Curve directCurve(data.data(), static_cast<int32_t>(data.size()));
	EXPECT_TRUE(directCurve.GetIsValid());
	EXPECT_TRUE(directCurve.GetControllPointCount() == 2);
	EXPECT_TRUE(directCurve.GetKnotCount() == 3);
	EXPECT_EQUAL_NEAR(directCurve.GetLength(), 2.0f, 0.0001f);
	EXPECT_EQUAL_NEAR(directCurve.CalcuratePoint(0.0f, 1.0f).X, 0.0f, 0.0001f);

	Effekseer::CurveLoader loader;
	auto loadedCurve = loader.Load(data.data(), static_cast<int32_t>(data.size()));
	EXPECT_TRUE(loadedCurve != nullptr);
	EXPECT_TRUE(loadedCurve->GetIsValid());

	const auto existingPath = GetDirectoryPathAsU16(__FILE__) + u"../../../../TestData/Effects/Curves/Curve1.efkcurve";
	auto existingData = LoadFile(existingPath.c_str());
	auto existingCurve = loader.Load(existingData.data(), static_cast<int32_t>(existingData.size()));
	EXPECT_TRUE(existingCurve != nullptr);
	EXPECT_TRUE(existingCurve->GetIsValid());
	EXPECT_TRUE(existingCurve->GetControllPointCount() == 10);
	EXPECT_TRUE(loader.Load(existingPath.c_str()) != nullptr);
}

void TestTruncatedCurveData()
{
	auto data = CreateCurveData();
	Effekseer::CurveLoader loader;
	for (size_t size = 0; size < data.size(); size++)
	{
		EXPECT_TRUE(loader.Load(data.data(), static_cast<int32_t>(size)) == nullptr);
		Effekseer::Curve directCurve(data.data(), static_cast<int32_t>(size));
		EXPECT_TRUE(!directCurve.GetIsValid());
		EXPECT_TRUE(directCurve.CalcuratePoint(0.5f, 1.0f).X == 0.0f);
	}

	EXPECT_TRUE(loader.Load(nullptr, 1) == nullptr);
	EXPECT_TRUE(loader.Load(data.data(), -1) == nullptr);
}

void TestInvalidCurveValues()
{
	constexpr size_t controlPointCountOffset = sizeof(int32_t);
	constexpr size_t firstControlPointOffset = sizeof(int32_t) * 2;
	constexpr size_t knotCountOffset = firstControlPointOffset + sizeof(Effekseer::dVector4) * 2;
	constexpr size_t firstKnotOffset = knotCountOffset + sizeof(int32_t);
	constexpr size_t tailOffset = firstKnotOffset + sizeof(double) * 3;
	Effekseer::CurveLoader loader;

	for (const auto& invalid : std::vector<std::pair<size_t, int32_t>>{
			 {0, 0},
			 {controlPointCountOffset, 0},
			 {controlPointCountOffset, -1},
			 {controlPointCountOffset, std::numeric_limits<int32_t>::max()},
			 {knotCountOffset, 0},
			 {knotCountOffset, -1},
			 {knotCountOffset, std::numeric_limits<int32_t>::max()},
			 {tailOffset, 2},
			 {tailOffset + sizeof(int32_t), 0},
			 {tailOffset + sizeof(int32_t) * 2, 3},
			 {tailOffset + sizeof(int32_t) * 3, 4},
		 })
	{
		auto data = CreateCurveData();
		Write(data, invalid.first, invalid.second);
		EXPECT_TRUE(loader.Load(data.data(), static_cast<int32_t>(data.size())) == nullptr);
	}

	for (const auto invalidValue : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
	{
		auto invalidPoint = CreateCurveData();
		Write(invalidPoint, firstControlPointOffset, invalidValue);
		EXPECT_TRUE(loader.Load(invalidPoint.data(), static_cast<int32_t>(invalidPoint.size())) == nullptr);

		auto invalidKnot = CreateCurveData();
		Write(invalidKnot, firstKnotOffset, invalidValue);
		EXPECT_TRUE(loader.Load(invalidKnot.data(), static_cast<int32_t>(invalidKnot.size())) == nullptr);
	}

	auto descendingKnots = CreateCurveData();
	Write(descendingKnots, firstKnotOffset + sizeof(double), -1.0);
	EXPECT_TRUE(loader.Load(descendingKnots.data(), static_cast<int32_t>(descendingKnots.size())) == nullptr);

	auto trailingData = CreateCurveData();
	trailingData.push_back(0);
	EXPECT_TRUE(loader.Load(trailingData.data(), static_cast<int32_t>(trailingData.size())) == nullptr);
}

TestRegister Runtime_BinaryReader_Validation("Runtime.BinaryReader.Validation", []() -> void
											 { TestBinaryReaderValidation(); });

TestRegister Runtime_Curve_Valid("Runtime.Curve.Valid", []() -> void
								 { TestValidCurveData(); });

TestRegister Runtime_Curve_Truncated("Runtime.Curve.Truncated", []() -> void
								 { TestTruncatedCurveData(); });

TestRegister Runtime_Curve_InvalidValues("Runtime.Curve.InvalidValues", []() -> void
										 { TestInvalidCurveValues(); });

} // namespace
