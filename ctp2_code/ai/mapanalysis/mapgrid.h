//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : The map grid
// Id           : $Id$
//
//----------------------------------------------------------------------------
//
// Disclaimer
//
// THIS FILE IS NOT GENERATED OR SUPPORTED BY ACTIVISION.
//
// This material has been developed at apolyton.net by the Apolyton CtP2
// Source Code Project. Contact the authors at ctp2source@apolyton.net.
//
//----------------------------------------------------------------------------
//
// Compiler flags
//
// - None
//
//----------------------------------------------------------------------------
//
// Modifications from the original Activision code:
//
// - None
//
//----------------------------------------------------------------------------

#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif
#ifndef __MAP_GRID_H__
#define __MAP_GRID_H__

#include <valarray>
#include <sstream>

#include "c3debugstl.h"
#include "c3debug.h"

#include "MapPoint.h"

#define RELAX_SHARED_SIDE (1.0)
#define RELAX_DIAGONAL (0.41)

template<class _Ty>
class MapGrid {
public:

	typedef std::valarray< _Ty > MapGridArray;

	MapGrid()
	{
		m_xSize = 0;
		m_ySize = 0;
		m_resolution = 0;

		Clear();
	}

	~MapGrid()
	{
		// A same-size (including 0 to 0) resize can still go through a
		// delete[]/new[] cycle in some valarray implementations, and this
		// project's operator new asserts against zero-size allocations -
		// so only resize down when there is actually something to free.
		if (s_scratch.size() != 0)
		{
			s_scratch.resize(0);
		}

		if (m_values.size() != 0)
		{
			m_values.resize(0);	// no free in valarray, and this is not really needed anyway
		}
	}

	void Resize(const sint32 & xSize,
		const sint32 & ySize,
		const sint32 & resolution)
	{

		m_xSize = xSize;
		m_ySize = ySize;
		m_resolution = resolution;

		m_xGridSize = (xSize / resolution)*2;
		m_yGridSize = (ySize / resolution);

		if (xSize / resolution != 0)
			m_xGridSize += 2;

		if (ySize / resolution != 0)
			m_yGridSize += 1;

		// Same reasoning as Cleanup()/~MapGrid(): a same-size resize (0 to
		// 0 in particular, e.g. when called before the world has valid
		// dimensions) can still trigger a zero-size allocation in some
		// valarray implementations, which this project's operator new
		// asserts against. Clear() (called by every caller of Resize())
		// already zeroes whatever is there, so skipping a no-op resize
		// changes nothing else.
		size_t const newSize =
		    static_cast<size_t>(m_xGridSize) * static_cast<size_t>(m_yGridSize);

		if (newSize == 0)
		{
			DPRINTF(k_DBG_AI,
			    ("MapGrid::Resize: computed a zero-size grid - xSize=%d ySize=%d resolution=%d xGridSize=%d yGridSize=%d oldValuesSize=%zu\n",
			     xSize, ySize, resolution, m_xGridSize, m_yGridSize, m_values.size()));
		}

		if (newSize != m_values.size())
		{
			m_values.resize(newSize);
		}
	}

	void Clear()
	{
		m_values = 0;

		m_maxGridValue = 0;
		m_minGridValue = 9999;
		m_maxValue = 0;
		m_minValue = 9999;
		m_valuesCount = 0;
		m_totalValue = 0.0;
	}

	void Cleanup()
	{
		Clear();

		// Same reasoning as the destructor: avoid resize(0) when already
		// empty, since that can still trigger a zero-size allocation in
		// some valarray implementations.
		if (m_values.size() != 0)
		{
			m_values.resize(0);
		}
	}

	void AddValue(const MapPoint &rc_pos, const _Ty & value)
	{
		MapPoint xy_pos;
		xy_pos.rc2xy(rc_pos, MapPoint(m_xSize, m_ySize));

		int const elem = ( (xy_pos.y / m_resolution) * m_xGridSize) +
			     (xy_pos.x / m_resolution);

		Assert(elem >= 0);
		Assert(static_cast<size_t>(elem) < m_values.size());

		Assert(((double) m_values[elem] + value) == (double) (m_values[elem] + value));
		m_values[elem] += value;

		Assert(((double) m_totalValue + value) == (double) (m_totalValue + value));
		m_totalValue += value;

		Assert(m_valuesCount >= 0);
		m_valuesCount++;

		if (value < m_minValue)
			m_minValue = value;
		if (value > m_maxValue)
			m_maxValue = value;

		if (m_values[elem] > m_maxGridValue)
			m_maxGridValue = m_values[elem];
		if (m_values[elem] < m_minGridValue)
			m_minGridValue = m_values[elem];
	}












	void Relax(sint32 cycles, const float coefficient)
	{

		if (cycles <= 0) return;

		MapGridArray *map_from_ptr = &m_values;
		MapGridArray *map_to_ptr = &s_scratch;

		s_scratch.resize(m_xGridSize*m_yGridSize);

		while(cycles-- > 0)
		{

			_Ty *pCurFrom = &((*map_from_ptr)[0]);
			_Ty *pCurTo = &((*map_to_ptr)[0]);


			sint32 up = (m_yGridSize-1)*m_xGridSize;
			sint32 down = m_xGridSize;
			for (sint32 y=0;y<m_yGridSize;++y) {

				if (y==m_yGridSize-1) down = -(m_yGridSize-1)*m_xGridSize;

				sint32 left = m_xGridSize-1;
				sint32 right = 1;
				for (sint32 x=0;x<m_xGridSize;++x) {

					if (x==m_xGridSize-1) right =  -(m_xGridSize-1);





					_Ty *pCur = pCurFrom + up + left;
					_Ty max_value = *pCur;

					pCur -= left;
					_Ty cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur += right;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur = pCurFrom+left;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur -= left;
					pCur += right;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur = pCurFrom + down + left;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur -= left;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;

					pCur += right;
					cur_value = *pCur;
					if (cur_value>max_value) max_value = cur_value;


					max_value = static_cast<_Ty>
						(static_cast<double>(max_value) *
						 RELAX_DIAGONAL *
						 coefficient
						);

     				cur_value = *pCurFrom;
					if (max_value > cur_value) cur_value = max_value;

					*pCurTo = cur_value;

					left = -1;

					++pCurFrom;
					++pCurTo;
				}

				up = -m_xGridSize;
			}

			MapGridArray *map_tmp_ptr;
			map_tmp_ptr = map_from_ptr;
			map_from_ptr = map_to_ptr;
			map_to_ptr = map_tmp_ptr;
		}




		if (map_from_ptr != &m_values)
			m_values = *map_from_ptr;
	}

#if 0

	void Relax(const sint8 & cycles, const float & coefficient)
	{
		sint32 elem;
		_Ty new_value;
		_Ty adj_value;
		bool rdiag;
		bool cdiag;
		sint8 delta_r, delta_c;

		if (cycles <= 0)
			return;

		MapGridArray *map_tmp_ptr;
		MapGridArray *map_from_ptr = &m_values;
		MapGridArray *map_to_ptr = &s_scratch;

		*map_to_ptr = *map_from_ptr;

		for (sint8 cycle = 0; cycle < cycles; cycle++)
		{
			for (elem = 0; elem < m_values.size(); elem++)
			{

				new_value = (*map_from_ptr)[elem];

				for (delta_r = -1; delta_r <= 1; delta_r++)
				{
					for (delta_c = -1; delta_c <= 1; delta_c++)
					{

						rdiag = (delta_r != 0);
						cdiag = (delta_c != 0);

						if ( !rdiag && !cdiag )
							continue;

						adj_value = GetRelaxValue( elem,
							delta_r,
							delta_c,
							*map_from_ptr);


						adj_value *= coefficient;


						new_value = (new_value > adj_value ? new_value : adj_value);
					}
				}

				(*map_to_ptr)[elem] = new_value;
			}

			map_tmp_ptr = map_from_ptr;
			map_from_ptr = map_to_ptr;
			map_to_ptr = map_tmp_ptr;
		}




		if (map_from_ptr != &m_values)
			m_values = *map_from_ptr;

	}
#endif




	_Ty GetGridValueXY(const MapPoint &xy_pos) const
	{
		int const elem =
			(1 == m_resolution)
			? (xy_pos.y * m_xGridSize) + xy_pos.x
			: ((xy_pos.y / m_resolution) * m_xGridSize) + (xy_pos.x / m_resolution);
		Assert(elem >= 0);
		Assert(static_cast<size_t>(elem) < m_values.size());

		return m_values[elem];
	}


	_Ty GetGridValue(const MapPoint &rc_pos) const
	{
		MapPoint xy_pos;
		xy_pos.rc2xy(rc_pos, MapPoint(m_xSize, m_ySize));

		int const elem = ((xy_pos.y / m_resolution) * m_xGridSize) +
			    (xy_pos.x / m_resolution);
		Assert(elem >= 0);
		Assert(static_cast<size_t>(elem) < m_values.size());

		return m_values[elem];
	}

	void SetGridValue(const MapPoint &rc_pos, const _Ty value)
	{
		MapPoint xy_pos;
		xy_pos.rc2xy(rc_pos, MapPoint(m_xSize, m_ySize));

		int const elem = ((xy_pos.y / m_resolution) * m_xGridSize) +
						 (xy_pos.x / m_resolution);
		Assert(elem >= 0);
		Assert(static_cast<size_t>(elem) < m_values.size());

		m_values[elem] = value;
	}


	_Ty GetGridValueFromXY(const MapPoint &xy_pos) const
	{
		int const elem = ((xy_pos.y / m_resolution) * m_xGridSize) +
						 (xy_pos.x / m_resolution);
		Assert(elem >= 0);
		Assert(static_cast<size_t>(elem) < m_values.size());

		return m_values[elem];
	}

	_Ty GetMaxGridValue() const { return m_maxGridValue; }

	_Ty GetMinGridValue() const { return m_minGridValue; }

	_Ty GetMaxValue() const { return m_maxValue; }

	_Ty GetMinValue() const { return m_minValue; }

	sint32 GetValuesCount() const { return m_valuesCount; }

	double GetTotalValue() const { return m_totalValue; }

	std::string GetDebugString() const
	{
		std::ostringstream ost;

		for (size_t elem = 0; elem < m_values.size(); ++elem)
		{
			ost.width( 8);
			ost << m_values[elem];
			ost << " ";

			if ( ((elem+1) % m_xGridSize) == 0)
				ost << "\n";
		}

		ost << "\n";
		return ost.str();
	}

private:

#if 0




	_Ty GetRelaxValue( const sint32 & src_elem,
		const sint8 & delta_x,
		const sint8 & delta_y,
		const MapGridArray & map_from ) const
	{






		bool diagonal = (delta_x != 0 || delta_y != 0);

		sint32 y = (sint32) (src_elem / m_xGridSize);
		sint32 x = (src_elem - ( y * m_xGridSize));

		x += delta_x;
		y += delta_y;




		if (y < 0) y = m_yGridSize - 1;
		if (x < 0) x = m_xGridSize - 1;
		if (y >= m_yGridSize) y = 0;
		if (x >= m_xGridSize) x = 0;

		Assert( (y * m_xGridSize) + x < map_from.size() );

		if (diagonal)
			return map_from[ (y * m_xGridSize) + x ] * RELAX_DIAGONAL;
		else
			return map_from[ (y * m_xGridSize) + x ] * RELAX_SHARED_SIDE;
	}
#endif

	sint32 m_xSize;

	sint32 m_ySize;

	sint32 m_xGridSize;

	sint32 m_yGridSize;

	sint32 m_resolution;

	static MapGridArray s_scratch;

	MapGridArray m_values;

	_Ty m_maxGridValue;

	_Ty m_minGridValue;

	_Ty m_maxValue;

	_Ty m_minValue;

	sint32 m_valuesCount;

	double m_totalValue;
};

#endif // __MAP_GRID_H
