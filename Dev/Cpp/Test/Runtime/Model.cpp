#include <Effekseer.h>

#include "../TestHelper.h"

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

std::vector<uint8_t> CreateModelDataVersion6()
{
	std::vector<uint8_t> data;
	const int32_t version = 6;
	const float scale = 1.0f;
	const int32_t modelCount = 1;
	const int32_t frameCount = 1;
	const int32_t vertexCount = 3;
	const int32_t faceCount = 1;

	Append(data, version);
	Append(data, scale);
	Append(data, modelCount);
	Append(data, frameCount);
	Append(data, vertexCount);

	for (int32_t i = 0; i < vertexCount; i++)
	{
		Effekseer::Model::Vertex vertex{};
		vertex.Position = Effekseer::Vector3D(static_cast<float>(i), 0.0f, 0.0f);
		vertex.VColor = Effekseer::Color(255, 255, 255, 255);
		Append(data, vertex);
	}

	Append(data, faceCount);
	Effekseer::Model::Face face{{0, 1, 2}};
	Append(data, face);
	return data;
}

std::vector<uint8_t> CreateModelDataVersion5()
{
	std::vector<uint8_t> data;
	const int32_t version = 5;
	const float scale = 1.0f;
	const int32_t modelCount = 1;
	const int32_t frameCount = 1;
	const int32_t vertexCount = 1;
	const int32_t faceCount = 0;
	const Effekseer::Vector3D vector3(1.0f, 2.0f, 3.0f);
	const Effekseer::Vector2D uv(0.25f, 0.75f);
	const Effekseer::Color color(1, 2, 3, 4);

	Append(data, version);
	Append(data, scale);
	Append(data, modelCount);
	Append(data, frameCount);
	Append(data, vertexCount);
	Append(data, vector3);
	Append(data, vector3);
	Append(data, vector3);
	Append(data, vector3);
	Append(data, uv);
	Append(data, color);
	Append(data, faceCount);
	return data;
}

void WriteInt32(std::vector<uint8_t>& data, size_t offset, int32_t value)
{
	EXPECT_TRUE(offset + sizeof(value) <= data.size());
	memcpy(data.data() + offset, &value, sizeof(value));
}

void TestValidModelData()
{
	auto data = CreateModelDataVersion6();
	Effekseer::Model model(data.data(), static_cast<int32_t>(data.size()));

	EXPECT_TRUE(model.GetIsValid());
	EXPECT_TRUE(model.GetFrameCount() == 1);
	EXPECT_TRUE(model.GetVertexCount() == 3);
	EXPECT_TRUE(model.GetFaceCount() == 1);
	const auto emitter = model.GetEmitterFromFace(0, 0, Effekseer::CoordinateSystem::RH, 1.0f);
	EXPECT_EQUAL_NEAR(emitter.Position.X, 1.0f, 0.0001f);

	auto oldData = CreateModelDataVersion5();
	Effekseer::Model oldModel(oldData.data(), static_cast<int32_t>(oldData.size()));
	EXPECT_TRUE(oldModel.GetIsValid());
	EXPECT_TRUE(oldModel.GetVertexCount() == 1);
	EXPECT_TRUE(oldModel.GetFaceCount() == 0);
	EXPECT_TRUE(oldModel.GetVertexes()[0].UV1.X == oldModel.GetVertexes()[0].UV2.X);
	EXPECT_TRUE(oldModel.GetVertexes()[0].UV1.Y == oldModel.GetVertexes()[0].UV2.Y);

	auto existingData = LoadFile((GetDirectoryPathAsU16(__FILE__) + u"../Resource/Model/block.efkmodel").c_str());
	Effekseer::Model existingModel(existingData.data(), static_cast<int32_t>(existingData.size()));
	EXPECT_TRUE(existingModel.GetIsValid());
	EXPECT_TRUE(existingModel.GetVertexCount() > 0);
	EXPECT_TRUE(existingModel.GetFaceCount() > 0);
}

void TestTruncatedModelData()
{
	auto data = CreateModelDataVersion6();
	for (size_t size = 0; size < data.size(); size++)
	{
		Effekseer::Model model(data.data(), static_cast<int32_t>(size));
		EXPECT_TRUE(!model.GetIsValid());
		EXPECT_TRUE(model.GetFrameCount() == 1);
		EXPECT_TRUE(model.GetVertexCount() == 0);
		EXPECT_TRUE(model.GetFaceCount() == 0);
	}

	Effekseer::Model nullModel(nullptr, 1);
	EXPECT_TRUE(!nullModel.GetIsValid());
	Effekseer::Model negativeSizeModel(data.data(), -1);
	EXPECT_TRUE(!negativeSizeModel.GetIsValid());
}

void TestInvalidModelCountsAndIndexes()
{
	constexpr size_t frameCountOffset = sizeof(int32_t) * 3;
	constexpr size_t vertexCountOffset = sizeof(int32_t) * 4;
	const size_t faceCountOffset = vertexCountOffset + sizeof(int32_t) + sizeof(Effekseer::Model::Vertex) * 3;
	const size_t faceOffset = faceCountOffset + sizeof(int32_t);

	for (const auto& invalid : std::vector<std::pair<size_t, int32_t>>{
			 {frameCountOffset, 0},
			 {frameCountOffset, -1},
			 {frameCountOffset, std::numeric_limits<int32_t>::max()},
			 {vertexCountOffset, -1},
			 {vertexCountOffset, std::numeric_limits<int32_t>::max()},
			 {faceCountOffset, -1},
			 {faceCountOffset, std::numeric_limits<int32_t>::max()},
		 })
	{
		auto data = CreateModelDataVersion6();
		WriteInt32(data, invalid.first, invalid.second);
		Effekseer::Model model(data.data(), static_cast<int32_t>(data.size()));
		EXPECT_TRUE(!model.GetIsValid());
	}

	for (int32_t index : {-1, 3, std::numeric_limits<int32_t>::max()})
	{
		auto data = CreateModelDataVersion6();
		WriteInt32(data, faceOffset, index);
		Effekseer::Model model(data.data(), static_cast<int32_t>(data.size()));
		EXPECT_TRUE(!model.GetIsValid());
	}

	auto tooManyFrames = CreateModelDataVersion6();
	WriteInt32(tooManyFrames, frameCountOffset, 65537);
	tooManyFrames.resize(frameCountOffset + sizeof(int32_t) + 65537 * sizeof(int32_t) * 2);
	Effekseer::Model tooManyFramesModel(tooManyFrames.data(), static_cast<int32_t>(tooManyFrames.size()));
	EXPECT_TRUE(!tooManyFramesModel.GetIsValid());
}

TestRegister Runtime_Model_Valid("Runtime.Model.Valid", []() -> void
								 { TestValidModelData(); });

TestRegister Runtime_Model_Truncated("Runtime.Model.Truncated", []() -> void
								 { TestTruncatedModelData(); });

TestRegister Runtime_Model_InvalidCountsAndIndexes("Runtime.Model.InvalidCountsAndIndexes", []() -> void
													 { TestInvalidModelCountsAndIndexes(); });

} // namespace
