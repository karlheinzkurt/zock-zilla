
#include "../include/CUnitCounter.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace Lib
{
namespace Controller
{
namespace Internal ///< Internal
{
   AUnitCounterBase::AUnitCounterBase(std::chrono::seconds limit, std::chrono::seconds cycle) : 
       m_limit(limit)
      ,m_cycle(cycle)
      ,m_active(0)
      ,m_previousRun() 
   {}
   
   AUnitCounterBase::AUnitCounterBase(std::chrono::seconds limit, std::chrono::seconds cycle, std::chrono::seconds active, time_point_type previousRun) :
       m_limit(limit)
      ,m_cycle(cycle)
      ,m_active(active)
      ,m_previousRun(previousRun) 
   {}
   
   std::string AUnitCounterBase::toString() const
   {
      std::ostringstream os;
      os << "Unit: "   << getUnit()        << ", "
         << "Limit: "  << m_limit.count()  << "s, "
         << "Active: " << m_active.count() << "s";
      return os.str();
   }
   
   bool AUnitCounterBase::exceedsLimit( time_point_type const& point ) const
   {
      return m_active >= m_limit;
   }
   
   std::chrono::seconds AUnitCounterBase::getLimit() const
   {
      return m_limit;
   }
   
   std::chrono::seconds AUnitCounterBase::getActive() const
   {
      return m_active;         
   }
   
   void AUnitCounterBase::doUpdate(time_point_type const& now)
   {
      /** Run timestamp gets updated in any case. 
       * */
      auto previousRun(getAndUpdate(m_previousRun, now));
      
      if (isNewUnit(now, previousRun))
      {
         /** First run in this unit, we just
          *  reset the active counter and leave.
          * */
         m_active = std::chrono::seconds( 0 );
         return;
      }
      
      if (!isConsecutiveRun(previousRun, now))
      {
         /** When this run is not directly
          *  following of a previous run, we
          *  just leave but we updated run 
          *  timestamp before.
          * */
         return;
      }
      
      /** A normal run adds the differnce to previous 
       *  run onto the active counter.
       */
      m_active += std::chrono::duration_cast<std::chrono::seconds>( now - previousRun );
   }

   bool AUnitCounterBase::isNewUnit(time_point_type const& pointA, time_point_type const& pointB)
   {
      auto const unitA(getUnitBegin(pointA));
      auto const unitB(getUnitBegin(pointB));
      return unitA > unitB;
   }
   
   IUnitCounter::time_point_type AUnitCounterBase::getAndUpdate(time_point_type& toUpdate, time_point_type point)
   {
      std::swap(toUpdate, point);
      return point;
   }
   
   bool AUnitCounterBase::isConsecutiveRun(time_point_type const& previous, time_point_type const& now)
   {
      return (previous >= (now - 1.5 * m_cycle));
   }
   
   boost::property_tree::ptree AUnitCounterBase::serialize() const
   {
      boost::property_tree::ptree ptCounter;
      std::ostringstream os;
      os << getUnit();
      ptCounter.put( "unit", os.str() );
      ptCounter.put( "limit", m_limit.count() );
      ptCounter.put( "active", m_active.count() );
      ptCounter.put( "previousRun", m_previousRun.time_since_epoch().count() );
      return std::move(ptCounter);
   }
   
   Unit CWeekCounter::getUnit() const
   {
      return Unit::Week;
   }
      
   IUnitCounter::time_point_type CWeekCounter::getUnitBegin(time_point_type const& point)
   {
      time_t const timeType(std::chrono::system_clock::to_time_t(point));
      tm const localTime(*localtime(&timeType));
      boost::gregorian::date date(boost::gregorian::date_from_tm(localTime));
      
      auto const startOfWeek(boost::gregorian::Monday); ///< Define start day of the week
      if (date.day_of_week() != startOfWeek)
      {  date = boost::gregorian::first_day_of_the_week_before(startOfWeek).get_date(date); }
      
      tm resultLocalTime = boost::gregorian::to_tm(date);
      resultLocalTime.tm_isdst = -1; ///< This is enough for our use case 
      resultLocalTime.tm_sec = resultLocalTime.tm_min = 0, resultLocalTime.tm_hour = 0;
      return std::chrono::system_clock::from_time_t(mktime(&resultLocalTime));
   }
   
   Unit CDayCounter::getUnit() const
   {
      return Unit::Day;
   }
   
   IUnitCounter::time_point_type CDayCounter::getUnitBegin(time_point_type const& point)
   {
      auto const timeType(std::chrono::system_clock::to_time_t(point));
      tm resultLocalTime(*localtime(&timeType));
      resultLocalTime.tm_isdst = -1; ///< This is enough for our use case
      resultLocalTime.tm_sec = resultLocalTime.tm_min = resultLocalTime.tm_hour = 0;
      return std::chrono::system_clock::from_time_t(mktime(&resultLocalTime));
   }
}}}
