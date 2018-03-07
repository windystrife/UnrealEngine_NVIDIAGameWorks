// nvtess.cpp : Defines the exported functions for the DLL application.
//

#include "nvtess.h"
#include <assert.h>

#define USE_HASHMAP 1
#define NO_STL 1

#if USE_HASHMAP
    #include <hash_map>
#else
    #include <map>
#endif

const unsigned int EdgesPerTriangle = 3;
const unsigned int IndicesPerTriangle = 3;
const unsigned int VerticesPerTriangle = 3;
const unsigned int DuplicateIndexCount = 3;

namespace nv {

#if NO_STL
	template <typename FromType, typename ToType>
	class FHashMap
	{
		static const size_t INVALID_ENTRY = (size_t)-1;
		struct FFromEntry
		{
			FromType From;
			size_t NextEntry;
		};

		size_t* HashTable;
		FFromEntry* FromEntries;
		ToType* ToEntries;
		size_t MaxEntries;
		size_t MaxHashes;
		size_t EntryCount;

	public:

		// These allow this class to stand in for the STL hash map where needed by this library.
		struct FToRef
		{
			const ToType& second;

			__forceinline explicit FToRef( const ToType* Ptr ) : second( *Ptr )
			{
			}

			__forceinline const FToRef* operator->() const
			{
				return this;
			}
		};

		struct iterator
		{
			const ToType* To;

			__forceinline explicit iterator( const ToType* Ptr ) : To( Ptr )
			{
			}

			__forceinline bool operator==( const iterator& Other ) const
			{
				return To == Other.To;
			}

			__forceinline bool operator!=( const iterator& Other ) const
			{
				return To != Other.To;
			}

			__forceinline const FToRef operator->() const
			{
				return FToRef( To );
			}
		};

		typedef iterator const_iterator;

		explicit FHashMap( const size_t InMaxEntries )
		{
			EntryCount = 0;
			MaxEntries = InMaxEntries;
			MaxHashes = (MaxEntries * 4) / 3;
			HashTable = new size_t[MaxHashes];
			FromEntries = new FFromEntry[MaxEntries];
			ToEntries = new ToType[MaxEntries];

			for ( size_t HashIndex = 0; HashIndex < MaxHashes; ++HashIndex )
			{
				HashTable[HashIndex] = INVALID_ENTRY;
			}
		}

		~FHashMap()
		{
			delete [] HashTable;
			delete [] FromEntries;
			delete [] ToEntries;
		}

		void set( const FromType& From, const ToType& To )
		{
			const size_t Hash = hash_value( From ) % MaxHashes;
			size_t EntryIndex = HashTable[Hash];
			while ( EntryIndex != INVALID_ENTRY )
			{
				const FFromEntry& Entry = FromEntries[EntryIndex];
				if ( Entry.From == From )
				{
					ToEntries[EntryIndex] = To;
					return;
				}
				EntryIndex = Entry.NextEntry;
			}
			assert( EntryCount < MaxEntries );
			FromEntries[EntryCount].NextEntry = HashTable[Hash];
			FromEntries[EntryCount].From = From;
			ToEntries[EntryCount] = To;
			HashTable[Hash] = EntryCount;
			EntryCount++;
		}

		__forceinline const iterator end() const
		{
			return iterator( 0 );
		}

		const iterator find( const FromType& From ) const
		{
			const size_t Hash = hash_value( From ) % MaxHashes;
			size_t EntryIndex = HashTable[Hash];
			while ( EntryIndex != INVALID_ENTRY )
			{
				const FFromEntry& Entry = FromEntries[EntryIndex];
				if ( Entry.From == From )
					return iterator( &ToEntries[EntryIndex] );
				EntryIndex = Entry.NextEntry;
			}
			return end();
		}
	};
#endif // #if NO_STL

    // ----------------------------------------------------------------------------------------
    // ----------------------------------------------------------------------------------------
    // ----------------------------------------------------------------------------------------------
    IndexBuffer::IndexBuffer(void* bufferContents, IndexBufferType ibtype, unsigned int length, bool bufferOwner) :
        mBufferContents(bufferContents),
        mIbType(ibtype),
        mLength(length),
        mBufferOwner(bufferOwner)
    { }

    // ----------------------------------------------------------------------------------------------
    IndexBuffer::~IndexBuffer() 
    { 
        if (mBufferOwner) { 
            delete[] mBufferContents; 
        } 

        mBufferContents = 0; 
    }

    // ----------------------------------------------------------------------------------------------
    unsigned int IndexBuffer::operator[](unsigned int index) const
    {
        switch (mIbType) {
        case IBT_U16: return ((unsigned short*)mBufferContents)[index]; break;
        case IBT_U32: return ((unsigned int*)mBufferContents)[index]; break;
        default: break;
        };

        assert(0);
        return 0;
    }

    // ----------------------------------------------------------------------------------------
    inline size_t hash_value(const Vector3& v3)
    {
        return 31337 * stdext::hash_value(v3.x) + 13 * stdext::hash_value(v3.y) + 3 * stdext::hash_value(v3.z);
    }

    // ----------------------------------------------------------------------------------------
    inline size_t hash_value(const Vertex& vert)
    {
        return hash_value(vert.pos);
    }

    namespace tess {
        // ----------------------------------------------------------------------------------------
        class Edge;

        inline size_t hash_value(const Edge& edge);

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        class Edge 
        {
            unsigned int mIndexFrom;
            unsigned int mIndexTo;

            nv::Vertex mVertexFrom;
            nv::Vertex mVertexTo;

            size_t mCachedHash;

        public:    
            Edge() : mCachedHash(0) { }

            Edge(unsigned int indexFrom, unsigned int indexTo, const nv::Vertex& vFrom, const nv::Vertex& vTo) :
            mIndexFrom(indexFrom), mIndexTo(indexTo), mVertexFrom(vFrom), mVertexTo(vTo)
            {
                // Hash should only consider position, not index. We want values with different indices to compare true.
                mCachedHash = 7 * hash_value(mVertexFrom) + 2 * hash_value(mVertexTo);
            }

            // ------------------------------------------------------------------------------------
            nv::Vertex vertex(unsigned int i) const
            {
                switch(i) { 
        case 0: return mVertexFrom; break;
        case 1: return mVertexTo; break;
        default: assert(0); break;
                }
                return nv::Vertex();
            }

            // ------------------------------------------------------------------------------------
            unsigned int index(unsigned int i) const
            { 
                switch(i) { 
        case 0: return mIndexFrom; break;
        case 1: return mIndexTo; break;
        default: assert(0); break;
                }
                return 0;
            };

            // ------------------------------------------------------------------------------------
            Edge reverse() const
            {
                return Edge(mIndexTo, mIndexFrom, mVertexTo, mVertexFrom);
            }

            // ------------------------------------------------------------------------------------
            // When comparing edges, compare the vertices--not the indices.
            __forceinline bool operator<(const Edge& rhs) const
            {
                // Quick out, otherwise we have to compare vertices. 
                if (mIndexFrom == rhs.mIndexFrom && mIndexTo == rhs.mIndexTo) { 
                    return false;
                }

                return mVertexFrom < rhs.mVertexFrom
                    || mVertexTo < rhs.mVertexTo;
            }

			__forceinline bool operator==( const Edge& Other ) const
			{
				return (mIndexFrom == Other.mIndexFrom && mIndexTo == Other.mIndexTo) ||
					(mVertexFrom == Other.mVertexFrom && mVertexTo == Other.mVertexTo);
			}

            friend size_t hash_value(const Edge& edge);
        };

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        struct Corner
		{
			unsigned int mIndex;
			nv::Vector2 mUV;

			Corner() : mIndex(0) {}

			Corner(unsigned int index, nv::Vector2 uv) : 
				mIndex(index), mUV(uv)
			{}
		};

        // ----------------------------------------------------------------------------------------
		#if NO_STL
			typedef FHashMap<Edge,Edge> EdgeDict;
			typedef FHashMap<nv::Vector3,Corner> PositionDict;
        #elif USE_HASHMAP
            typedef stdext::hash_map<Edge, Edge> EdgeDict;
			typedef stdext::hash_map<nv::Vector3, Corner> PositionDict;
        #else
            typedef std::map<Edge, Edge> EdgeDict;
            typedef std::map<nv::Vector3, Corner> PositionDict;
        #endif

        typedef EdgeDict::iterator EdgeDictIt;
        typedef PositionDict::iterator PositionDictIt;

		void add_if_leastUV(PositionDict& outPd, const nv::Vertex& v, unsigned int i)
		{
			PositionDictIt foundIt = outPd.find(v.pos);
			if(foundIt == outPd.end())
			{
#if NO_STL
				outPd.set( v.pos, Corner(i, v.uv) );
#else // #if NO_STL
				outPd[v.pos] = Corner(i, v.uv);
#endif // #if NO_STL
			}
			else if(v.uv < foundIt->second.mUV)
			{
#if NO_STL
				outPd.set( v.pos, Corner(i, v.uv) );
#else // #if NO_STL
				outPd[v.pos] = Corner(i, v.uv);
#endif // #if NO_STL
			}
		}

        // ----------------------------------------------------------------------------------------
        inline size_t hash_value(const Edge& edge)
        {    
            return edge.mCachedHash;
        }

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        class Triangle
        {
            Edge mEdge0;
            Edge mEdge1;
            Edge mEdge2;

        public:
            inline const Edge& edge(unsigned int i) const { return ((Edge*)&mEdge0)[i]; }
            inline unsigned int index(unsigned int i) const { return edge(i).index(0); }

            // ------------------------------------------------------------------------------------
            Triangle(unsigned int i0, unsigned int i1, unsigned int i2, const nv::Vertex& v0, const nv::Vertex& v1, const nv::Vertex& v2) :
            mEdge0(i0, i1, v0, v1),
                mEdge1(i1, i2, v1, v2),
                mEdge2(i2, i0, v2, v0)
            { }

            // ------------------------------------------------------------------------------------
            bool operator<(const Triangle& rhs) const
            { 
                return mEdge0 < rhs.mEdge0 
                    || mEdge1 < rhs.mEdge1 
                    || mEdge2 < rhs.mEdge2; 
            }
        };

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
		template <typename ElemType>
		IndexBuffer* createIndexBuffer(const unsigned int* newIndices, unsigned int indexCount, IndexBufferType destBufferType)
		{
			ElemType* elemBuffer = new ElemType[indexCount];

			for (unsigned int u = 0; u < indexCount; ++u) {
				elemBuffer[u] = (ElemType)newIndices[u];
			}

			return new IndexBuffer((void*)elemBuffer, destBufferType, indexCount, true);
		}

        // ----------------------------------------------------------------------------------------
        IndexBuffer* newIndexBuffer(const std::vector<unsigned int>& newIndices, const RenderBuffer* inputBuffer)
        {
            IndexBufferType ibType = inputBuffer->getIb()->getType();

            switch (ibType) {
        case IBT_U16: return createIndexBuffer<unsigned short>(&newIndices[0], (unsigned int)newIndices.size(), IBT_U16); break;
        case IBT_U32: return createIndexBuffer<unsigned int>(&newIndices[0], (unsigned int)newIndices.size(), IBT_U32); break;
        default: assert(0); break;
            };

            return 0;
        }

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        void expandIb_DominantEdgeAndCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex)
        {
            outIb[startOutIndex +  0] = tri.index(0);
            outIb[startOutIndex +  1] = tri.index(1);
            outIb[startOutIndex +  2] = tri.index(2);

            outIb[startOutIndex +  3] = tri.index(0);
            outIb[startOutIndex +  4] = tri.index(1);
            outIb[startOutIndex +  5] = tri.index(1);
            outIb[startOutIndex +  6] = tri.index(2);
            outIb[startOutIndex +  7] = tri.index(2);
            outIb[startOutIndex +  8] = tri.index(0);

            outIb[startOutIndex +  9] = tri.index(0);
            outIb[startOutIndex + 10] = tri.index(1);
            outIb[startOutIndex + 11] = tri.index(2);
        }

        // ----------------------------------------------------------------------------------------
        void expandIb_PnAenOnly(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex)
        {
            outIb[startOutIndex + 0] = tri.index(0);
            outIb[startOutIndex + 1] = tri.index(1);
            outIb[startOutIndex + 2] = tri.index(2);

            outIb[startOutIndex + 3] = tri.index(0);
            outIb[startOutIndex + 4] = tri.index(1);
            outIb[startOutIndex + 5] = tri.index(1);
            outIb[startOutIndex + 6] = tri.index(2);
            outIb[startOutIndex + 7] = tri.index(2);
            outIb[startOutIndex + 8] = tri.index(0);
        }

        // ----------------------------------------------------------------------------------------
        void expandIb_PnAenDominantCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex)
        {
            outIb[startOutIndex +  0] = tri.index(0);
            outIb[startOutIndex +  1] = tri.index(1);
            outIb[startOutIndex +  2] = tri.index(2);

            outIb[startOutIndex +  3] = tri.index(0);
            outIb[startOutIndex +  4] = tri.index(1);
            outIb[startOutIndex +  5] = tri.index(1);
            outIb[startOutIndex +  6] = tri.index(2);
            outIb[startOutIndex +  7] = tri.index(2);
            outIb[startOutIndex +  8] = tri.index(0);

            outIb[startOutIndex +  9] = tri.index(0);
            outIb[startOutIndex + 10] = tri.index(1);
            outIb[startOutIndex + 11] = tri.index(2);
        }

        // ----------------------------------------------------------------------------------------
        void expandIb_PnAenDominantEdgeAndCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex)
        {
            outIb[startOutIndex +  0] = tri.index(0);
            outIb[startOutIndex +  1] = tri.index(1);
            outIb[startOutIndex +  2] = tri.index(2);

            outIb[startOutIndex +  3] = tri.index(0);
            outIb[startOutIndex +  4] = tri.index(1);
            outIb[startOutIndex +  5] = tri.index(1);
            outIb[startOutIndex +  6] = tri.index(2);
            outIb[startOutIndex +  7] = tri.index(2);
            outIb[startOutIndex +  8] = tri.index(0);

            outIb[startOutIndex +  9] = tri.index(0);
            outIb[startOutIndex + 10] = tri.index(1);
            outIb[startOutIndex + 11] = tri.index(1);
            outIb[startOutIndex + 12] = tri.index(2);
            outIb[startOutIndex + 13] = tri.index(2);
            outIb[startOutIndex + 14] = tri.index(0);

            outIb[startOutIndex + 15] = tri.index(0);
            outIb[startOutIndex + 16] = tri.index(1);
            outIb[startOutIndex + 17] = tri.index(2);
        }

        // ----------------------------------------------------------------------------------------
        void expandIb(unsigned int* outIb, DestBufferMode destBufferMode, EdgeDict& outEd, PositionDict& outPd, const RenderBuffer* inputBuffer)
        {
            const IndexBuffer* inIb = inputBuffer->getIb();
            const unsigned int triCount = inIb->getLength() / IndicesPerTriangle;
            const unsigned int outputIndicesPerPatch = getIndicesPerPatch(destBufferMode);

            bool requiresEdgeDict = true;
            bool requiresPositionDict = destBufferMode != DBM_PnAenOnly;

            for (unsigned int u = 0; u < triCount; ++u) {

                const unsigned int startInIndex = u * IndicesPerTriangle;
                const unsigned int startOutIndex = u * outputIndicesPerPatch;

                const unsigned int i0 = (*inIb)[startInIndex + 0], 
                    i1 = (*inIb)[startInIndex + 1], 
                    i2 = (*inIb)[startInIndex + 2];

                const Vertex v0 = inputBuffer->getVertex(i0),
                    v1 = inputBuffer->getVertex(i1),
                    v2 = inputBuffer->getVertex(i2);

                Triangle tri(i0, i1, i2, v0, v1, v2);

                switch (destBufferMode) {
                    case DBM_DominantEdgeAndCorner:
                        expandIb_DominantEdgeAndCorner(outIb, tri, startOutIndex);
                        break;
                    case DBM_PnAenOnly:
                        expandIb_PnAenOnly(outIb, tri, startOutIndex);
                        break;
                    case DBM_PnAenDominantCorner:
                        expandIb_PnAenDominantCorner(outIb, tri, startOutIndex);
                        break;
                    case DBM_PnAenDominantEdgeAndCorner:
                        expandIb_PnAenDominantEdgeAndCorner(outIb, tri, startOutIndex);
                        break;
                    default:
                        assert(0);
                        break;
                };


                if (requiresEdgeDict) {
                    Edge rev0 = tri.edge(0).reverse(),
                         rev1 = tri.edge(1).reverse(),
                         rev2 = tri.edge(2).reverse();
#if NO_STL
					outEd.set( rev0, rev0 );
					outEd.set( rev1, rev1 );
					outEd.set( rev2, rev2 );
#else // #if NO_STL
                    outEd[rev0] = rev0;
                    outEd[rev1] = rev1;
                    outEd[rev2] = rev2;
#endif // #if NO_STL
                }

                if (requiresPositionDict) {
                    add_if_leastUV(outPd, v0, i0);
					add_if_leastUV(outPd, v1, i1);
					add_if_leastUV(outPd, v2, i2);
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        void replacePlaceholderIndices_DominantEdgeAndCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex, const EdgeDict& edgeDict, const PositionDict& posDict)
        {
            // First, compute dominant edges. We define dominant edges to be the edge that has either the lower 
            // minimum index or the lower maximum index. This designation is purely capricious, but it is stable.
            for (unsigned int u = 0; u < EdgesPerTriangle; ++u) {
                EdgeDict::const_iterator fwdIt = edgeDict.find(tri.edge(u));
                EdgeDict::const_iterator revIt = edgeDict.find(tri.edge(u).reverse());
                EdgeDict::const_iterator cit = edgeDict.end();

                if (fwdIt != edgeDict.end() && revIt != edgeDict.end()) {
                    unsigned int eFmin = std::min(fwdIt->second.index(0), fwdIt->second.index(1));
                    unsigned int eFmax = std::max(fwdIt->second.index(0), fwdIt->second.index(1));
                    unsigned int eRmin = std::min(revIt->second.index(0), revIt->second.index(1));
                    unsigned int eRmax = std::max(revIt->second.index(0), revIt->second.index(1));

                    if (eFmin < eRmin) {
                        cit = fwdIt;
                    } else if (eRmin < eFmin) {
                        cit = revIt;
                    } else if (eFmax < eRmax) {
                        cit = fwdIt;
                    } else if (eRmax < eFmax) {
                        // Could actually fold this and the final case together,
                        // but this is logically easier to understand
                        cit = revIt;
                    } else {
                        // In this case, the indices are the same--so it doesn't matter what we choose.
                        cit = revIt;
                    }
                } else if (fwdIt != edgeDict.end()) {
                    cit = fwdIt;
                } else if (revIt != edgeDict.end()) {
                    cit = revIt;
                }

                if (cit != edgeDict.end()) {
                    outIb[startOutIndex + 3 + 2 * u] = cit->second.index(0);
                    outIb[startOutIndex + 4 + 2 * u] = cit->second.index(1);
                }
            }

            // Dominant Positions are much easier.
            for (unsigned u = 0; u < VerticesPerTriangle; ++u) {
				PositionDict::const_iterator pit = posDict.find(tri.edge(u).vertex(0).pos);
                if (pit != posDict.end()) {
					outIb[startOutIndex + 9 + u] = pit->second.mIndex;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        void replacePlaceholderIndices_PnAenOnly(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex, const EdgeDict& edgeDict, const PositionDict& posDict)
        {
            EdgeDict::const_iterator cit = edgeDict.find(tri.edge(0));

            if (cit != edgeDict.end()) {
                outIb[startOutIndex + 3] = cit->second.index(0);
                outIb[startOutIndex + 4] = cit->second.index(1);
            }

            cit = edgeDict.find(tri.edge(1));
            if (cit != edgeDict.end()) {
                outIb[startOutIndex + 5] = cit->second.index(0);
                outIb[startOutIndex + 6] = cit->second.index(1);
            }

            cit = edgeDict.find(tri.edge(2));
            if (cit != edgeDict.end()) {
                outIb[startOutIndex + 7] = cit->second.index(0);
                outIb[startOutIndex + 8] = cit->second.index(1);
            }
        }

        // ----------------------------------------------------------------------------------------
        void replacePlaceholderIndices_PnAenDominantCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex, const EdgeDict& edgeDict, const PositionDict& posDict)
        {
            replacePlaceholderIndices_PnAenOnly(outIb, tri, startOutIndex, edgeDict, posDict);

            // Deal with dominant positions.
            for (unsigned u = 0; u < VerticesPerTriangle; ++u) {
                PositionDict::const_iterator pit = posDict.find(tri.edge(u).vertex(0).pos);
                if (pit != posDict.end()) {
					outIb[startOutIndex + 9 + u] = pit->second.mIndex;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        void replacePlaceholderIndices_PnAenDominantEdgeAndCorner(unsigned int* outIb, const Triangle& tri, unsigned int startOutIndex, const EdgeDict& edgeDict, const PositionDict& posDict)
        {
            replacePlaceholderIndices_PnAenOnly(outIb, tri, startOutIndex, edgeDict, posDict);
            replacePlaceholderIndices_DominantEdgeAndCorner(outIb, tri, startOutIndex + getIndicesPerPatch(DBM_PnAenOnly) - VerticesPerTriangle, edgeDict, posDict);
        }

        // ----------------------------------------------------------------------------------------
        // TODO: Remove the input buffer, replace with a cached copy of the "vertex" buffer
        void replacePlaceholderIndices(unsigned int* outIb, 
									   unsigned int indexCount,
                                       DestBufferMode destBufferMode, 
                                       const EdgeDict& edgeDict, 
                                       const PositionDict& posDict, 
                                       const RenderBuffer* inputBuffer)
        {
            const unsigned int outputIndicesPerPatch = getIndicesPerPatch(destBufferMode);
            const unsigned int triCount = indexCount / outputIndicesPerPatch;

            for (unsigned int u = 0; u < triCount; ++u) {
                const unsigned int startOutIndex = u * outputIndicesPerPatch;

                const unsigned int i0 = outIb[startOutIndex + 0], 
                    i1 = outIb[startOutIndex + 1], 
                    i2 = outIb[startOutIndex + 2];

                const Vertex v0 = inputBuffer->getVertex(i0),
                    v1 = inputBuffer->getVertex(i1),
                    v2 = inputBuffer->getVertex(i2);

                Triangle tri(i0, i1, i2, v0, v1, v2); 

                switch (destBufferMode) {
                    case DBM_DominantEdgeAndCorner:
                        replacePlaceholderIndices_DominantEdgeAndCorner(outIb, tri, startOutIndex, edgeDict, posDict);
                        break;
                    case DBM_PnAenOnly:
                        replacePlaceholderIndices_PnAenOnly(outIb, tri, startOutIndex, edgeDict, posDict);
                        break;
                    case DBM_PnAenDominantCorner:
                        replacePlaceholderIndices_PnAenDominantCorner(outIb, tri, startOutIndex, edgeDict, posDict);
                        break;
                    case DBM_PnAenDominantEdgeAndCorner:
                        replacePlaceholderIndices_PnAenDominantEdgeAndCorner(outIb, tri, startOutIndex, edgeDict, posDict);
                        break;
                    default:
                        assert(0);
                        break;
                };
            }
        } 

        // ----------------------------------------------------------------------------------------
        void stripUnusedIndices(std::vector<unsigned int>& outIb, DestBufferMode destBufferMode)
        {
            const unsigned int indicesPerPatch = getIndicesPerPatch(destBufferMode);
            const unsigned int destIndicesPerPatch = indicesPerPatch - DuplicateIndexCount;

            const unsigned int numPatches = (unsigned int) outIb.size() / indicesPerPatch;
            const unsigned int newIbSize = numPatches * destIndicesPerPatch;

            std::vector<unsigned int> newIb(newIbSize);

            unsigned int sourceIndex = DuplicateIndexCount;
			const unsigned int * __restrict srcIndices = &outIb[0];
			unsigned int * __restrict destIndices = &newIb[0];
            for (unsigned int destIndex = 0; destIndex < newIbSize; ++destIndex) {
                destIndices[destIndex] = srcIndices[sourceIndex++];
                if (sourceIndex % indicesPerPatch == 0) {
                    sourceIndex += DuplicateIndexCount;
                }
            }

            // Pointer swap.
            outIb.swap(newIb);
        }

        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        // ----------------------------------------------------------------------------------------
        IndexBuffer* buildTessellationBuffer(const RenderBuffer* inputBuffer, DestBufferMode destBufferMode, bool completeBuffer)
        {
#if NO_STL
			EdgeDict edgeDict(inputBuffer->getIb()->getLength());
			PositionDict posDict(inputBuffer->getIb()->getLength());
#else // #if NO_STL
            EdgeDict edgeDict;
            PositionDict posDict;
            #if USE_HASHMAP
                edgeDict.rehash(inputBuffer->getIb()->getLength() * EdgesPerTriangle / IndicesPerTriangle * 2 + 1);
                posDict.rehash(inputBuffer->getIb()->getLength() * VerticesPerTriangle / IndicesPerTriangle * 2 + 1);
            #endif
#endif // #if NO_STL

            std::vector<unsigned int> newIb(getIndicesPerPatch(destBufferMode) * inputBuffer->getIb()->getLength() / IndicesPerTriangle);
            expandIb(&newIb[0], destBufferMode, edgeDict, posDict, inputBuffer);

            replacePlaceholderIndices(&newIb[0], (unsigned int)newIb.size(), destBufferMode, edgeDict, posDict, inputBuffer);
            if (!completeBuffer) {
                stripUnusedIndices(newIb, destBufferMode);
            }

            return newIndexBuffer(newIb, inputBuffer);
        }
    };
};
