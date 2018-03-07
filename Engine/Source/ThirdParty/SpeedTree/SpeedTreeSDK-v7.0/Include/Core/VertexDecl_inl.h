///////////////////////////////////////////////////////////////////////
//  VertexDecl.inl
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  Constants

const st_int32 c_nObjectVertexStream = 0;
const st_int32 c_nInstanceVertexStream = 1;

const SVertexDecl::SAttribDesc c_asBillboardInstanceStreamDesc[ ] =
{
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_2, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// instance pos.x
	      { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// instance pos.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// instance pos.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// instance scalar
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_3, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// up vector.x
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// up vector.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// up vector.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// pad
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_4, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// right vector.x
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// right vector.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// right vector.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// pad
	VERTEX_DECL_END( )
};

const SVertexDecl::SAttribDesc c_as3dTreeInstanceStreamDesc[ ] =
{
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_1, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// instance pos.x
	      { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// instance pos.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// instance pos.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// instance scalar
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_2, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// up vector.x
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// up vector.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// up vector.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// LOD transition
	{ c_nInstanceVertexStream, VERTEX_ATTRIB_3, VERTEX_FORMAT_FULL_FLOAT, 4,
		{ { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_X },		// right vector.x
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Y },		// right vector.y
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_Z },		// right vector.z
		  { VERTEX_PROPERTY_MISC_SEMANTIC, VERTEX_COMPONENT_W } } },	// LOD value
	VERTEX_DECL_END( )
};

static const SVertexDecl::SAttribDesc* c_asGrassInstanceStreamDesc = c_as3dTreeInstanceStreamDesc;


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::SAttribute

ST_INLINE SVertexDecl::SAttribute::SAttribute( ) :
	m_uiStream(0),
	m_eFormat(VERTEX_FORMAT_UNASSIGNED)
{
	Clear( );
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::IsUsed

ST_INLINE st_bool SVertexDecl::SAttribute::IsUsed(void) const
{
	return (m_aeProperties[VERTEX_COMPONENT_X] != VERTEX_PROPERTY_UNASSIGNED ||
			m_aeProperties[VERTEX_COMPONENT_Y] != VERTEX_PROPERTY_UNASSIGNED ||
			m_aeProperties[VERTEX_COMPONENT_Z] != VERTEX_PROPERTY_UNASSIGNED ||
			m_aeProperties[VERTEX_COMPONENT_W] != VERTEX_PROPERTY_UNASSIGNED);
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::NumEmptyComponents

ST_INLINE st_int32 SVertexDecl::SAttribute::NumEmptyComponents(void) const
{
	return (m_aeProperties[VERTEX_COMPONENT_X] == VERTEX_PROPERTY_UNASSIGNED ? 1 : 0) +
		   (m_aeProperties[VERTEX_COMPONENT_Y] == VERTEX_PROPERTY_UNASSIGNED ? 1 : 0) +
		   (m_aeProperties[VERTEX_COMPONENT_Z] == VERTEX_PROPERTY_UNASSIGNED ? 1 : 0) +
		   (m_aeProperties[VERTEX_COMPONENT_W] == VERTEX_PROPERTY_UNASSIGNED ? 1 : 0);
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::NumUsedComponents

ST_INLINE st_int32 SVertexDecl::SAttribute::NumUsedComponents(void) const
{
	return VERTEX_COMPONENT_COUNT - NumEmptyComponents( );
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::Size

ST_INLINE st_int32 SVertexDecl::SAttribute::Size(void) const
{
	return NumUsedComponents( ) * SVertexDecl::FormatSize(m_eFormat);
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::Clear

ST_INLINE void SVertexDecl::SAttribute::Clear(void)
{
	for (st_int32 i = 0; i < VERTEX_COMPONENT_COUNT; ++i)
	{
		m_aeProperties[i] = VERTEX_PROPERTY_UNASSIGNED;
		m_aePropertyComponents[i] = VERTEX_COMPONENT_UNASSIGNED;
		m_auiVertexOffsets[i] = 0;
	}
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SAttribute::FirstFreeComponent

ST_INLINE EVertexComponent SVertexDecl::SAttribute::FirstFreeComponent(void) const
{
	for (st_int32 i = 0; i < VERTEX_COMPONENT_COUNT; ++i)
		if (m_aeProperties[i] == -1)
			return EVertexComponent(i);

	assert(false);

	return VERTEX_COMPONENT_X;
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::SProperty::SProperty

ST_INLINE SVertexDecl::SProperty::SProperty( ) :
	m_eFormat(VERTEX_FORMAT_UNASSIGNED)
{
	for (st_int32 i = 0; i < VERTEX_COMPONENT_COUNT; ++i)
	{
		m_aeAttribs[i] = VERTEX_ATTRIB_UNASSIGNED;
		m_aeAttribComponents[i] = VERTEX_COMPONENT_UNASSIGNED;
		m_auiOffsets[i] = 0;
	}
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::SProperty::IsPresent

ST_INLINE st_bool SVertexDecl::SProperty::IsPresent(void) const
{
	return (NumComponents( ) > 0);
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::SProperty::IsContiguous

ST_INLINE st_bool SVertexDecl::SProperty::IsContiguous(void) const
{
	st_bool bContiguous = false;

	if (IsPresent( ))
	{
		bContiguous = true;

		const st_int32 c_nNumComponents = NumComponents( );
		st_int32 nSlot = m_aeAttribs[0];
		for (st_int32 i = 1; i < c_nNumComponents; ++i)
		{
			if (m_aeAttribs[i] != nSlot)
			{
				bContiguous = false;
				break;
			}
		}
	}

	return bContiguous;
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::SProperty::NumComponents

ST_INLINE st_int32 SVertexDecl::SProperty::NumComponents(void) const
{
	st_int32 nNumComponents = 0;

	for (st_int32 i = 0; i < VERTEX_COMPONENT_COUNT; ++i)
		if (m_aeAttribs[i] == VERTEX_ATTRIB_UNASSIGNED)
			break;
		else
			++nNumComponents;

	return nNumComponents;
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::SVertexDecl

ST_INLINE SVertexDecl::SVertexDecl( ) :
	m_uiVertexSize(0)
{
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::operator!=

ST_INLINE st_bool SVertexDecl::operator!=(const SVertexDecl& sRight) const
{
    return (memcmp(m_asAttributes, sRight.m_asAttributes, sizeof(m_asAttributes)) != 0);
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::operator<

ST_INLINE st_bool SVertexDecl::operator<(const SVertexDecl& sRight) const
{
    return (memcmp(m_asAttributes, sRight.m_asAttributes, sizeof(m_asAttributes)) < 0);
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::FormatName

ST_INLINE const char* SVertexDecl::FormatName(EVertexFormat eFormat)
{
	const char* c_pNames[VERTEX_FORMAT_COUNT] = 
	{
		"full float", "half float", "byte"
	};

	return c_pNames[eFormat];
}


///////////////////////////////////////////////////////////////////////
//  SVertexDecl::FormatSize

ST_INLINE st_int32 SVertexDecl::FormatSize(EVertexFormat eFormat)
{
	const st_int32 c_anSizesInBytes[VERTEX_FORMAT_COUNT] =
	{
		4, // VERTEX_FORMAT_FULL_FLOAT
		2, // VERTEX_FORMAT_HALF_FLOAT
		1  // VERTEX_FORMAT_BYTE
	};

	return c_anSizesInBytes[eFormat];
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::AttributeName

ST_INLINE const char* SVertexDecl::AttributeName(EVertexAttribute eAttrib)
{
	const char* c_pNames[VERTEX_ATTRIB_COUNT] = 
	{
		"ATTR0",//POSITION",
		"ATTR1",//BLENDWEIGHT",
		"ATTR2",//NORMAL",
		"ATTR3",//COLOR0",
		"ATTR4",//COLOR1",
		"ATTR5",//TANGENT",
		"ATTR6",//PSIZE",
		"ATTR7",//BLENDINDICES",
		"ATTR8",//TEXCOORD0",
		"ATTR9",//TEXCOORD1",
		"ATTR10",//TEXCOORD2",
		"ATTR11",//TEXCOORD3",
		"ATTR12",//TEXCOORD4",
		"ATTR13",//TEXCOORD5",
		"ATTR14",//TEXCOORD6",
		"ATTR15"//TEXCOORD7"
	};

	return c_pNames[eAttrib];
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::GetDescription

ST_INLINE void SVertexDecl::GetDescription(CString& strDesc) const
{
	const st_char* c_pFormatNames[VERTEX_FORMAT_COUNT] =
	{
		"32-bit floats", "16-bit floats", "byte"
	};

	//strDesc = CString::Format("vertex size: %d bytes\n", m_uiVertexSize);
	for (st_int32 nAttrib = 0; nAttrib < VERTEX_ATTRIB_COUNT; ++nAttrib)
	{
		const SAttribute& sAttrib = m_asAttributes[nAttrib];

		if (sAttrib.IsUsed( ))
		{
			// get attribute name
			strDesc += CString::Format("  %s [ ", AttributeName(EVertexAttribute(nAttrib)));

			// print an entry for each element
			for (st_int32 i = 0; i < sAttrib.NumUsedComponents( ); ++i)
			{
				strDesc += CString::Format("%s.%s(%d) ",
										   CCore::GetVertexPropertyDesc(sAttrib.m_aeProperties[i]).m_pShortName,
										   CCore::ComponentName(sAttrib.m_aePropertyComponents[i]),
										   sAttrib.m_auiVertexOffsets[i]);
			}

			// get data format
			strDesc += CString::Format("] (%s)\n", c_pFormatNames[sAttrib.m_eFormat]);
		}
	}
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::GetInstanceVertexDecl

ST_INLINE void SVertexDecl::GetInstanceVertexDecl(SVertexDecl& sInstanceDecl, EInstanceType eInstanceType)
{
	switch (eInstanceType)
	{
	case INSTANCES_3D_TREES:
		sInstanceDecl.Set(c_as3dTreeInstanceStreamDesc);
		break;
	case INSTANCES_GRASS:
		sInstanceDecl.Set(c_asGrassInstanceStreamDesc);
		break;
	case INSTANCES_BILLBOARDS:
		sInstanceDecl.Set(c_asBillboardInstanceStreamDesc);
		break;
	case INSTANCES_NONE:
		// do nothing
		break;
	default:
		assert(false);
	}
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::MergeObjectAndInstanceVertexDecls

ST_INLINE st_bool SVertexDecl::MergeObjectAndInstanceVertexDecls(SVertexDecl& sMergedDecl, const SVertexDecl& sObjectDecl, EInstanceType eInstanceType)
{
	st_bool bSuccess = true;

	// start with object decl
	sMergedDecl = sObjectDecl;

	if (eInstanceType != INSTANCES_NONE)
	{
		SVertexDecl sInstanceDecl;
		GetInstanceVertexDecl(sInstanceDecl, eInstanceType);

		// add instance decl
		for (st_int32 i = 0; i < VERTEX_ATTRIB_COUNT; ++i)
		{
			const SVertexDecl::SAttribute& sInstAttrib = sInstanceDecl.m_asAttributes[i];
			if (sInstAttrib.IsUsed( ))
			{
				SVertexDecl::SAttribute& sDestAttrib = sMergedDecl.m_asAttributes[i];
				if (!sDestAttrib.IsUsed( ))
				{
					sDestAttrib = sInstAttrib;
				}
				else
				{
					CCore::SetError("SVertexDecl::MergeObjectAndInstanceVertexDecls, overlapping instance and object vertex declarations");
					bSuccess = false;
					break;
				}
			}
		}

		// adjust merged vertex size
		sMergedDecl.m_uiVertexSize += sInstanceDecl.m_uiVertexSize;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  SVertexDecl::Set

ST_INLINE st_bool SVertexDecl::Set(const SAttribDesc* pAttribDesc)
{
	assert(pAttribDesc);

	st_bool bSuccess = false;

	// count # of entries & compute vertex size
	m_uiVertexSize = 0;
	const SAttribDesc* pAttrib = pAttribDesc;
	while (pAttrib->m_eAttrib != VERTEX_ATTRIB_COUNT)
	{
		// set attributes
		SAttribute& sDeclAttrib = m_asAttributes[pAttrib->m_eAttrib];

		sDeclAttrib.m_uiStream = pAttrib->m_uiStream;
		sDeclAttrib.m_eFormat = pAttrib->m_eFormat;
		for (st_uint16 i = 0; i < pAttrib->m_uiNumComponents; ++i)
		{
			sDeclAttrib.m_aeProperties[i] = pAttrib->m_asProperties[i].m_eProperty;
			sDeclAttrib.m_aePropertyComponents[i] = pAttrib->m_asProperties[i].m_eComponent;
			sDeclAttrib.m_auiVertexOffsets[i] = m_uiVertexSize;

			// set properties
			SProperty& sDeclProperty = m_asProperties[pAttrib->m_asProperties[i].m_eProperty];
			sDeclProperty.m_eFormat = pAttrib->m_eFormat;
			sDeclProperty.m_aeAttribs[pAttrib->m_asProperties[i].m_eComponent] = pAttrib->m_eAttrib;
			sDeclProperty.m_aeAttribComponents[pAttrib->m_asProperties[i].m_eComponent] = EVertexComponent(i);
			sDeclProperty.m_auiOffsets[pAttrib->m_asProperties[i].m_eComponent] = m_uiVertexSize;

			m_uiVertexSize += st_uint8(SVertexDecl::FormatSize(pAttrib->m_eFormat));
		}

		++pAttrib;
	}

	return bSuccess;
}

