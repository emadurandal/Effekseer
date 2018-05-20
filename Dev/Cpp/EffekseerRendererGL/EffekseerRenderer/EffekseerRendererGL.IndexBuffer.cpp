
//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
#include "EffekseerRendererGL.IndexBuffer.h"
#include "EffekseerRendererGL.GLExtension.h"

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
namespace EffekseerRendererGL
{
//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
IndexBuffer::IndexBuffer( RendererImplemented* renderer, GLuint buffer, int maxCount, bool isDynamic )
	: DeviceObject		( renderer )
	, IndexBufferBase	( maxCount, isDynamic )
	, m_buffer			( buffer )
{
	m_resource = new uint8_t[m_indexMaxCount * sizeof(uint16_t)];
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
IndexBuffer::~IndexBuffer()
{
	delete [] m_resource;
	GLExt::glDeleteBuffers(1, &m_buffer);
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
IndexBuffer* IndexBuffer::Create( RendererImplemented* renderer, int maxCount, bool isDynamic )
{
	GLuint ib;
	GLExt::glGenBuffers(1, &ib);

	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
	GLExt::glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxCount * sizeof(uint16_t), nullptr, GL_STREAM_DRAW);
	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return new IndexBuffer( renderer, ib, maxCount, isDynamic );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void IndexBuffer::OnLostDevice()
{
	GLExt::glDeleteBuffers(1, &m_buffer);
	m_buffer = 0;
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void IndexBuffer::OnResetDevice()
{
	if (IsValid()) return;
	GLuint ib;
	GLExt::glGenBuffers(1, &ib);
	m_buffer = ib;

	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
	GLExt::glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(uint16_t), nullptr, GL_STREAM_DRAW);
	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void IndexBuffer::Lock()
{
	assert( !m_isLock );

	m_isLock = true;
	m_indexCount = 0;

	m_offset = 0;
	m_lockedSize = 0;
}

bool IndexBuffer::RingBufferLock(int32_t size, int32_t& offset, void*& data)
{
	assert(!m_isLock);

	if (m_offset + size >= m_indexMaxCount * sizeof(uint16_t))
	{
		if (size >= m_indexMaxCount * sizeof(uint16_t)) return false;
		offset = 0;
		data = m_resource;
		m_offset = 0;
		m_lockedSize = size;
		m_isLock = true;
	}

	offset = m_offset;
	data = m_resource;
	m_lockedSize = size;
	m_isLock = true;
	return true;
}

bool IndexBuffer::TryRingBufferLock(int32_t size, int32_t& offset, void*& data)
{
	assert(!m_isLock);

	if (m_offset + size >= m_indexMaxCount * sizeof(uint16_t)) return false;
	
	offset = m_offset;
	data = m_resource;
	m_lockedSize = size;
	m_isLock = true;
	return true;
}


//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void IndexBuffer::Unlock()
{
	assert( m_isLock );

	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);

	if (m_lockedSize == 0)
	{
		//GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
		GLExt::glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m_indexCount * sizeof(uint16_t), m_resource);
		//GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	else if (GLExt::IsSupportedBufferRange())
	{
		auto target = GLExt::glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, m_offset, m_lockedSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		memcpy(target, m_resource, m_lockedSize);
		GLExt::glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
	else
	{
		GLExt::glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, m_offset, m_lockedSize, m_resource);
	}

	m_offset += m_lockedSize;

	GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
	//GLExt::glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(uint16_t), m_resource, GL_DYNAMIC_DRAW);
	//GLExt::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	m_isLock = false;
}


bool IndexBuffer::IsValid()
{
	return m_buffer != 0;
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
