
#ifndef NVTESS_H
#define NVTESS_H

namespace nv {
    enum DestBufferMode
    {
        /** 
        This buffer contains dominant Edge and Corner Information for each vertex,
        suitable for performing crack-free displacement mapping on top of flat tessellation.
        */
        DBM_DominantEdgeAndCorner = 0,

        /** 
        This buffer contains indices needed to use with PN-AEN, but does not contain
        information to support crack-free displacement mapping.
        */
        DBM_PnAenOnly = 1,

        /**
        This buffer contains PN-AEN information along with dominant corner information. This
        information is sufficient to perform crack-free displacement mapping
        */
        DBM_PnAenDominantCorner = 2,

        /**
        This buffer contains PN-AEN information along with dominant corner and edge information.
        Although dominant edges can be inferred from PN-AEN information alone (making this mode 
        somewhat bloated compared to DBM_PnAenDominantCorner), it does remove the need for some
        computation in the Hull Shader.
        */
        DBM_PnAenDominantEdgeAndCorner = 3,

        DestBufferMode_MAX
    };

    enum IndexBufferType
    {
        IBT_U16,  // 16-bit indices
        IBT_U32,  // 32-bit indices

        IndexBufferType_MAX
    };

    struct Vector3
    {
        float x, y, z;

        inline bool operator==(const Vector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
        inline bool operator<(const Vector3& rhs) const { return  x < rhs.x || y < rhs.y || z < rhs.z; }
    };

	struct Vector2
    {
        float x, y;

        inline bool operator==(const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
		inline bool operator<(const Vector2& rhs) const { return x < rhs.x || (x == rhs.x && y < rhs.y); }
    };

    struct Vertex
    {
        nv::Vector3 pos;
		nv::Vector2 uv;

        inline bool operator==(const Vertex& rhs) const { return pos == rhs.pos; }
        inline bool operator<(const Vertex& rhs) const { return pos < rhs.pos; }
    };

    /**
    This is a simple wrapper class around an index buffer. The index buffer must contain triangle lists,
    strips are not currently supported. This class will be used to pass index buffers into the library 
    as well as be used for the mechanism to return the created indices back to the application.

    The IndexBuffer class can either own the specified void* pointer (it will be freed by calling delete),
    or the application can indicate that it will retain ownership.
    */
    class IndexBuffer
    {
    private:
        void* mBufferContents;
        IndexBufferType mIbType;
        unsigned int mLength;
        bool mBufferOwner;

    public:
        IndexBuffer(void* bufferContents, IndexBufferType ibtype, unsigned int length, bool bufferOwner);
        ~IndexBuffer();

        inline IndexBufferType getType() const { return mIbType; }
        inline unsigned int getLength() const { return mLength; }
        unsigned int operator[](unsigned int index) const; 
    };

    class RenderBuffer
    {
    protected:
        IndexBuffer* mIb;

    public:
        inline virtual ~RenderBuffer() { delete mIb; }

        /** Return the vertex information at the specified index. */
        virtual nv::Vertex getVertex(unsigned int index) const = 0;

        const IndexBuffer* getIb() const { return mIb; }
    };


    namespace tess {    
        inline unsigned int getIndicesPerPatch(DestBufferMode destBufferMode)
        {
            switch (destBufferMode) {
                case DBM_DominantEdgeAndCorner:			return 12;
                case DBM_PnAenOnly:						return 9;
                case DBM_PnAenDominantCorner:			return 12;
                case DBM_PnAenDominantEdgeAndCorner:	return 18;
                default: // todo: Error
                    break;
            }

            return 3;
        }

        /** 
        Returns an index buffer suitable for the technique specified by destBufferMode. 

        Ownership of the return buffer belongs to the application and will need to be freed using
        delete.
        @destBufferMode - One of the modes specified in DestBufferMode.
        @completeBuffer - Whether the buffer returned should have all indices. If true, all 
        */
        IndexBuffer* buildTessellationBuffer(const RenderBuffer* inputBuffer, DestBufferMode destBufferMode, bool completeBuffer);
    };
};

#endif /* NVTESS_H */
