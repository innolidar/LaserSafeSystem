#include "ProtectionManager.h"
#include "GeoUtils.h"
#include <cmath>
#include <iostream>
#include <QFileInfo>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * 方法 1: 使用 double 返回，垂直线返回无穷大 (infinity)
 * @param angleDegrees 角度值（单位：度）
 * @return 斜率 k。如果直线垂直于 x 轴，返回 std::numeric_limits<double>::infinity()
 */
double calculateSlope(double angleDegrees) {
    // 1. 将角度标准化到 [0, 360) 范围，防止负数或超大角度影响判断
    // fmod 处理浮点数取模
    double normalizedAngle = std::fmod(angleDegrees, 360.0);
    if (normalizedAngle < 0) {
        normalizedAngle += 360.0;
    }

    // 2. 判断是否为垂直线 (90度 或 270度)
    // 由于浮点数精度问题，不能直接用 == 90.0，需要设置一个极小的误差范围 epsilon
    const double epsilon = 1e-9;
    bool isVertical = (std::abs(normalizedAngle - 90.0) < epsilon) || 
                      (std::abs(normalizedAngle - 270.0) < epsilon);

    if (isVertical) {
        return std::numeric_limits<double>::infinity(); // 或者返回 NaN，视业务需求而定
    }

    // 3. 角度转弧度
    double angleRadians = normalizedAngle * M_PI / 180.0;

    // 4. 计算正切值即为斜率 k
    return std::tan(angleRadians);
}
#pragma pack(push, 1)
typedef struct                                                                                                          
{                                                                                                                       
    /*固定字段*/                                                                                                        
    uint32_t header{0xAA55AA55};                                                                                                    
    /* bank个数固定为0x10 */                                                                                            
    uint16_t bank_num{0x10};                                                                                                  
    /* zone个数固定为0x03 */                                                                                            
    uint16_t zone_num{0x03};                                                                                                  
    /* 多重采样字段 */                                                                                                  
    uint8_t multi_sample[48]{0};                                                                                           
    /* 目标分辨率字段 */                                                                                                
    uint8_t target_sample_rate[48]{0};                                                                                     
    /* 区域采样角度间隔 仅支持 25表示0.25°、 50表示0.5° 、100表示1° */                                                  
    uint16_t zone_sample_rate{25};                                                                                          
    /* 区域起始角度 */                                                                                                  
    uint16_t zone_startAngle[48]{0};                                                                                       
    /* 区域结束角度 */                                                                                                  
    uint16_t zone_endAngle[48]{0};                                                                                         
    /* 区域形状类型 0 表示扇形  1表示多边形*/                                                                           
    uint8_t zone_Shapetype[48]{0};                                                                                         
    /* 区域扇形长度 若为扇形，表示扇形半径 */                                                                           
    uint8_t zone_sectorlenth[48]{0};                                                                                       
    /* 数据区 */                                                                                                        
    uint8_t data[0x3E00]{0};                                                                                        
}HA25ZoneProt_DataType;
typedef struct                                                                                                          
{                                                                                                                       
    /*固定字段*/                                                                                                        
    uint32_t header{0xAA55AA55};                                                                                                    
    /* bank个数固定为0x10 */                                                                                            
    uint16_t bank_num{0x10};                                                                                                  
    /* zone个数固定为0x03 */                                                                                            
    uint16_t zone_num{0x03};                                                                                                  
    /* 多重采样字段 */                                                                                                  
    uint8_t multi_sample[48]{0};                                                                                           
    /* 目标分辨率字段 */                                                                                                
    uint8_t target_sample_rate[48]{0};                                                                                     
    /* 区域采样角度间隔 仅支持 25表示0.25°、 50表示0.5° 、100表示1° */                                                  
    uint16_t zone_sample_rate{25};                                                                                          
    /* 区域起始角度 */                                                                                                  
    uint16_t zone_startAngle[48]{0};                                                                                       
    /* 区域结束角度 */                                                                                                  
    uint16_t zone_endAngle[48]{0};                                                                                         
    /* 区域形状类型 0 表示扇形  1表示多边形*/                                                                           
    uint8_t zone_Shapetype[48]{0};                                                                                         
    /* 区域扇形长度 若为扇形，表示扇形半径 */                                                                           
    uint8_t zone_sectorlenth[48]{0};                                                                                       
    /* 数据区 */                                                                                                        
    uint8_t data[0x3E00]{0};                                                                                        
}HA50ZoneProt_DataType; 
#pragma pack(pop)

struct LineParam
{
    int start_angle{0};
    int end_angle{0};
    float k{0.f};
    float b{0.f};
};

class BinManager
{
private:
    BinManager()
    {

    }
    ~BinManager()
    {

    }
private:
    bool HA25Save(const QString& file_name,const QVector<ProtectionZone> &zones)
    {
        QVector<ProtectionZone> temp_zones=zones;
        HA25ZoneProt_DataType data;
        std::cout<<"HA25ZoneProt_DataType len:"<<sizeof(HA25ZoneProt_DataType)<<std::endl;
        //0x3E00 HA25
        QVector<ProtectionZone> tem_zones{};
        int data_25_count{0};
        int data_50_count{0};
        int data_100_count{0};
        for(auto& zone:temp_zones)
        {
            if(zone.shape!=ProtectionZone::Shape::Sector)
            {
                /* 区域扇形长度 若为扇形，表示扇形半径 */
                size_t size=zone.points.size();
                int start_angle{0};
                int end_angle{0};
                if(zone.shape==ProtectionZone::Shape::Polygon)
                {
                    start_angle=GeoUtils::cartesianToPolar(zone.points[1]).second*100;
                    end_angle=GeoUtils::cartesianToPolar(zone.points[size-2]).second*100;
                }
                else if(zone.shape==ProtectionZone::Shape::Rectangle)
                {
                    start_angle=GeoUtils::cartesianToPolar(zone.points[0]).second*100;
                    end_angle=GeoUtils::cartesianToPolar(zone.points[1]).second*100;
                }
                
                if(start_angle>end_angle)
                {
                    int tem=start_angle;
                    start_angle=end_angle;
                    end_angle=tem;
                }
                zone.startAngle=start_angle;
                zone.endAngle=end_angle;
                
                //int end_angle=zone.endAngle*100;
                std::cout<<"start_angle:"<<start_angle<<std::endl;
                std::cout<<"end_angle:"<<end_angle<<std::endl;
                int start_25_count=start_angle/25;
                int end_25_count=end_angle/25+((end_angle%25)?1:0);
                
                int value_25=end_25_count*25-start_25_count*25;
                data_25_count+=value_25/25+((value_25%25)?1:0)+1;
                
                int start_50_count=start_angle/50-((start_angle%50)?1:0);
                int end_50_count=end_angle/50-((end_angle%50)?1:0);
                int value_50=end_50_count*50-start_50_count*50;
                data_50_count+=value_50/50+((value_50%50)?1:0)+1;

                int start_100_count=start_angle/100-((start_angle%100)?1:0);
                int end_100_count=end_angle/100-((end_angle%100)?1:0);
                int value_100=end_100_count*100-start_100_count*100;
                data_100_count+=value_100/100+((value_100%100)?1:0)+1;
            }                                                                                                 
        }
        int rate_type{0};
        if(data_25_count<=0x3E00)
        {
            rate_type=0;
            data.zone_sample_rate=25;
        }
        else if(data_50_count<=0x3E00)
        {
            rate_type=1;
            data.zone_sample_rate=50;
        }
        else if(data_100_count<=0x3E00)
        {
            rate_type=2;
            data.zone_sample_rate=100;
        }
        else
        {
            return false;
        }
        int index{0};
        for(auto& zone:temp_zones)
        {
            std::cout<<"id:"<<zone.id<<std::endl;
            int id=zone.id-1;
            data.multi_sample[id]=zone.multiSampling;
            data.target_sample_rate[id]=zone.targetResolution;
            /* 区域起始角度 */  
            
            /* 区域形状类型 0 表示扇形  1表示多边形*/
            data.zone_Shapetype[id]=zone.shape==ProtectionZone::Shape::Sector?0:1;
            if(zone.shape==ProtectionZone::Shape::Sector)
            {
                data.zone_startAngle[id]=zone.startAngle*100;
                data.zone_endAngle[id]=zone.endAngle*100; 
                /* 区域扇形长度 若为扇形，表示扇形半径 */    
                data.zone_sectorlenth[id]=zone.radius*10;
            }
            else
            {
                int start_angle=zone.startAngle;
                int end_angle=zone.endAngle;
                std::cout<<"start_angle:"<<start_angle<<" end_angle:"<<end_angle<<std::endl;
                switch(rate_type)
                {
                case 0:
                {
                    int start_count=start_angle/25;
                    int end_count=end_angle/25+((end_angle%25)?1:0);
                    
                    data.zone_startAngle[id]=start_count*25;
                    data.zone_endAngle[id]=end_count*25;
                    std::vector<LineParam> lines{};
                    GetLineParams(lines,zone.points,zone.shape,25);
                    for(int angle=data.zone_startAngle[id];angle<=end_count*25;angle+=25)
                    {
                        int value=GetTheoryDistance(angle,lines);
                        data.data[index++]=(uint8_t)GetTheoryDistance(angle,lines);
                    }
                }
                break;
                case 1:
                {
                    int start_count=start_angle/50-((start_angle%50)?1:0);
                    int end_count=end_angle/50-((end_angle%50)?1:0);
                    data.zone_startAngle[id]=start_count*50;
                    data.zone_endAngle[id]=end_count*50;
                    std::vector<LineParam> lines{};
                    GetLineParams(lines,zone.points,zone.shape,50);
                    for(int angle=data.zone_startAngle[id];angle<=end_count*50;angle+=50)
                    {
                        int value=GetTheoryDistance(angle,lines);
                        data.data[index++]=(uint8_t)GetTheoryDistance(angle,lines);
                    }
                }
                break;
                case 2:
                {
                    int start_count=start_angle/100-((start_angle%100)?1:0);
                    int end_count=end_angle/100-((end_angle%100)?1:0);
                    data.zone_startAngle[id]=start_count*100;
                    data.zone_endAngle[id]=end_count*100;
                    std::vector<LineParam> lines{};
                    GetLineParams(lines,zone.points,zone.shape,100);
                    for(int angle=data.zone_startAngle[id];angle<=end_count*100;angle+=100)
                    {
                        int value=GetTheoryDistance(angle,lines);
                        data.data[index++]=(uint8_t)value;
                    }
                }
                break;
                default:
                break;
                }
            }                                                                                                      
        }
        std::cout<<"index:"<<index<<" data_25_count:"<<data_25_count<<" data_50_count:"<<data_50_count<<" data_100_count:"<<data_100_count<<std::endl;
        QFile bin_file(file_name);
        // 二进制只写模式打开
        if (!bin_file.open(QIODevice::WriteOnly)) 
        {
            //qDebug() << "文件打开失败：" << bin_file.errorString();
            return -1;
        }
        // 整块写入结构体内存
        qint64 writeLen = bin_file.write(reinterpret_cast<const char*>(&data), sizeof(HA25ZoneProt_DataType));
        if (writeLen != sizeof(HA25ZoneProt_DataType)) 
        {
            //qDebug() << "写入失败，预期" << sizeof(ZoneProt_DataType) << "字节，实际写入" << writeLen << "字节";
        }
        bin_file.close();
        return true;
    }
    bool HA50Save(const QString& file_name,const QVector<ProtectionZone> &zones)
    {
        return false;
    }

    void GetLineParams(std::vector<LineParam> &lines,const QVector<QPointF> &points,const ProtectionZone::Shape shape,const int &rate_flag=25)
    {
        QVector<QPointF> pts=points;
        size_t size=pts.size();
        
        if(size>=4)
        {
            if(shape==ProtectionZone::Shape::Polygon)
            {
                if(pts[0]==pts[size-1] && pts[0]==QPointF{0,0})
                {
                    pts.pop_front();
                    pts.pop_back();
                }
            }
            else if(shape==ProtectionZone::Shape::Rectangle)
            {
                auto point_0=pts[1];
                auto point_1=pts[2];
                auto point_2=pts[3];
                auto point_3=pts[0];
                pts[0]=point_0;
                pts[1]=point_1;
                pts[2]=point_2;
                pts[3]=point_3;
            }
            int start_angle=GeoUtils::cartesianToPolar(pts[0]).second*100;
            start_angle=(start_angle/rate_flag)*rate_flag;
            size=pts.size();
            for(int i=1;i<size;i++)
            {
                LineParam param{};
                param.start_angle=start_angle;
                param.end_angle=GeoUtils::cartesianToPolar(pts[i]).second*100;
                if(i==size-1)
                {
                    param.end_angle=(param.end_angle/rate_flag+((param.end_angle%rate_flag)?1:0))*rate_flag;
                }
                if((pts[i].x()-pts[i-1].x())==0.0)
                {
                    param.k=9999999;
                    param.b=pts[i].x();
                }
                else
                {
                    param.k=(pts[i].y()-pts[i-1].y())/(pts[i].x()-pts[i-1].x());
                    param.b=pts[i].y()-pts[i].x()*param.k;
                }
                if(param.start_angle>param.end_angle)
                {
                    auto temp=param.start_angle;
                    param.start_angle=param.end_angle;
                    param.end_angle=temp;
                }
                std::cout<<"param.start_angle:"<<param.start_angle<<" param.end_angle:"<<param.end_angle<<" k:"<<param.k<<" b:"<<param.b<<std::endl;
                lines.push_back(param);
                start_angle=param.end_angle;
            }
        }
    }
    int GetTheoryDistance(const int &angle,const std::vector<LineParam> &lines)
    {
        float k{0};
        float x{0};
        float y{0};
        bool     is_find{false};
        LineParam param{};
        if(angle==9000 || angle==27000)
        {
            for(auto &line:lines)
            {
                if(angle>=line.start_angle && angle<=line.end_angle)
                {
                    is_find=true;
                    param=line;
                    /*if(line.k!=9999999)
                    {
                        y=0;
                        if(line.k!=0)
                        {
                            x=(y-line.b)/line.k;
                        }
                        
                    }
                    else if(line.k==9999999)
                    {
                        y=0;
                        x=line.b;
                    }*/
                    y=0;
                    x=line.b;

                    break;
                }
            }
        }
        else if(angle==18000)
        {
            for(auto &line:lines)
            {
                if(angle>=line.start_angle && angle<=line.end_angle)
                {
                    is_find=true;
                    param=line;
                    if(line.k!=0)
                    {
                        x=0;
                        y=line.k*x+line.b;
                    }
                    break;
                }
            }
        }
        else
        {
            k=calculateSlope(float(angle-9000)*0.01);
            for(auto &line:lines)
            {
                if(angle>=line.start_angle && angle<=line.end_angle)
                {
                    is_find=true;
                    param=line;
                    if(line.k!=9999999)
                    {
                        if(line.k==0)
                        {
                            y=line.b;
                            x=y/k;
                        }
                        else
                        {
                            x=line.b/(k-line.k);
                            y=line.k*x+line.b;
                        }
                    }
                    else if(line.k==9999999)
                    {
                        x=line.b;
                        y=k*x;
                    }
                    break;
                }
            }
        }
        if(is_find)
        {
            std::cout<<"angle:"<<angle<<" x:"<<x<<" y:"<<y<<" d:"<<std::sqrt(x*x+y*y)*10<<std::endl;
            std::cout<<"line start_angle:"<<param.start_angle<<" end_angle:"<<param.end_angle<<" k:"<<param.k<<" b:"<<param.b<<std::endl;
        }
        
        return std::sqrt(x*x+y*y)*10;
    }
public:
    static BinManager& GetInstance()
    {
        static BinManager manager;
        return manager;
    }
    bool Save(const QString& file_name,const QVector<ProtectionZone> &zones,const bool &is_ha25=true)
    {
        if(is_ha25)
        {
            return HA25Save(file_name,zones);
        }
        return HA50Save(file_name,zones);
        
        return true;
    }
};
// ===========================================================================
// ProtectionManager
// ===========================================================================
ProtectionManager::ProtectionManager(QObject *parent)
    : QObject(parent)
{

}

int ProtectionManager::addZone(const ProtectionZone &zone)
{
    ProtectionZone z = zone;
    //m_next_id++;
    z.id = z.bankIndex*3+z.outputIndex;
    if (z.name.isEmpty())
    {
        z.name = QString("Bank%1-OUT%2").arg(z.bankIndex).arg(z.outputIndex);
    }
    // Bank/输出索引合法性检查
    if (z.bankIndex < 0 || z.bankIndex >= kBankCount)
    {
        z.bankIndex = 0;
    }  
    if (z.outputIndex < 1 || z.outputIndex > kOutputCount)
    {
        z.outputIndex = nextFreeOutput(z.bankIndex);
    }

    // 同一个 Bank+Output 下只保留一个区域：移除旧区域（替换绑定）
    for (int i = m_zones.size() - 1; i >= 0; --i)
    {
        if (m_zones[i].bankIndex == z.bankIndex &&
            m_zones[i].outputIndex == z.outputIndex)
        {
            int oldId = m_zones[i].id;
            m_zones.removeAt(i);
            if (m_active_id == oldId)
            {
                m_active_id = -1;
            }
            emit zoneRemoved(oldId);
        }
    }
    m_zones.append(z);
    emit zoneAdded(z.id);
    emit zonesChanged();
    return z.id;
}

int ProtectionManager::addPolygon(const QVector<QPointF> &points, int bank, int output)
{
    ProtectionZone z;
    z.shape  = ProtectionZone::Polygon;
    z.points = points;
    z.bankIndex = bank;
    z.outputIndex = output;
    return addZone(z);
}

int ProtectionManager::addCircle(const QPointF &center, double radius, int bank, int output)
{
    ProtectionZone z;
    z.shape  = ProtectionZone::Circle;
    z.radius = radius;
    z.bankIndex = bank;
    z.outputIndex = output;

    // 圆形：以圆心为极点的完整圆
    const int segs = 64;
    for (int i = 0; i < segs; ++i) 
    {
        double a = 360.0 * double(i) / double(segs);
        QPointF p = GeoUtils::polarToCartesian(radius, a);
        z.points.append(center + p);
    }
    return addZone(z);
}

int ProtectionManager::addRectangle(const QPointF &center, double w, double h, double angle,int bank, int output)
{
    ProtectionZone z;
    z.shape  = ProtectionZone::Rectangle;
    z.width  = w;
    z.height = h;
    z.angle  = angle;
    z.bankIndex = bank;
    z.outputIndex = output;

    double rad = qDegreesToRadians(angle);
    double cosA = qCos(rad), sinA = qSin(rad);
    double hw = w / 2.0, hh = h / 2.0;
    QPointF corners[4] = {
        QPointF(-hw, -hh), QPointF(hw, -hh), QPointF(hw, hh), QPointF(-hw, hh)
    };
    for (int i = 0; i < 4; ++i) 
    {
        double rx = corners[i].x() * cosA - corners[i].y() * sinA;
        double ry = corners[i].x() * sinA + corners[i].y() * cosA;
        z.points.append(center + QPointF(rx, ry));
    }
    return addZone(z);
}

int ProtectionManager::addSector(double startAngle, double endAngle, double radius,int bank, int output)
{
    ProtectionZone z;
    z.shape  = ProtectionZone::Sector;
    z.startAngle = GeoUtils::alignAngle(startAngle);
    z.endAngle   = GeoUtils::alignAngle(endAngle);
    z.radius = radius;
    z.bankIndex = bank;
    z.outputIndex = output;
    double s = GeoUtils::normalizeAngle(startAngle);
    double e = GeoUtils::normalizeAngle(endAngle);
    double sweep = e - s;
    if (sweep <= 0.0)
    {
        sweep += 360.0;
    }
    const int segs = qMax(8, int(sweep / 3.0) + 1);
    {
        double a = s + sweep * double(0) / double(segs);
        z.points.append(GeoUtils::polarToCartesian(radius, a));
    }
    {
        double a = s + sweep * double(segs) / double(segs);
        z.points.append(GeoUtils::polarToCartesian(radius, a));
    }
    return addZone(z);
}

bool ProtectionManager::updateZone(int id, const ProtectionZone &zone)
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].id == id) 
        {
            ProtectionZone updated = zone;
            updated.id = id;
            m_zones[i] = updated;
            emit zoneUpdated(id);
            emit zonesChanged();
            return true;
        }
    }
    return false;
}

bool ProtectionManager::removeZone(int id)
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].id == id) 
        {
            m_zones.removeAt(i);
            if (m_active_id == id) 
            {
                m_active_id = -1;
                emit activeZoneChanged(m_active_id);
            }
            emit zoneRemoved(id);
            emit zonesChanged();
            return true;
        }
    }
    return false;
}

void ProtectionManager::clearZones()
{
    m_zones.clear();
    m_active_id = -1;
    emit activeZoneChanged(m_active_id);
    emit zonesChanged();
}

QVector<ProtectionZone> ProtectionManager::zones() const
{
    return m_zones;
}

ProtectionZone *ProtectionManager::zone(int id)
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].id == id)
        {
            return &m_zones[i];
        }
    }
    return nullptr;
}

const ProtectionZone *ProtectionManager::zone(int id) const
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].id == id)
        {
            return &m_zones[i];
        }  
    }
    return nullptr;
}

ProtectionZone *ProtectionManager::zoneAt(int bank, int output)
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].bankIndex == bank && m_zones[i].outputIndex == output)
        {
            return &m_zones[i];
        }    
    }
    return nullptr;
}

QVector<ProtectionZone> ProtectionManager::zonesOfBank(int bank) const
{
    QVector<ProtectionZone> result;
    for (const ProtectionZone &z : m_zones) 
    {
        if (z.bankIndex == bank)
        {
            result.append(z);
        }
    }
    return result;
}

int ProtectionManager::activeZoneId() const
{
    return m_active_id;
}

void ProtectionManager::setActiveZoneId(int id)
{
    if (m_active_id == id)
    {
        return;
    }
    m_active_id = id;
    emit activeZoneChanged(m_active_id);
}

ProtectionZone *ProtectionManager::activeZone()
{
    return zone(m_active_id);
}

int ProtectionManager::enabledZoneCount() const
{
    int n = 0;
    for (const ProtectionZone &z : m_zones)
    {
        if (z.enabled)
        {
            ++n;
        }   
    }
    return n;
}

int ProtectionManager::nextFreeOutput(int bank) const
{
    for (int out = 1; out <= kOutputCount; ++out) 
    {
        if (!zoneAtConst(bank, out))
        {
            return out;
        }
    }
    return 1;
}

const ProtectionZone *ProtectionManager::zoneAtConst(int bank, int output) const
{
    for (int i = 0; i < m_zones.size(); ++i) 
    {
        if (m_zones[i].bankIndex == bank && m_zones[i].outputIndex == output)
        {
            return &m_zones[i];
        } 
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 复制 / 擦除
// ---------------------------------------------------------------------------
int ProtectionManager::copyActiveZone()
{
    if (m_active_id < 0)
    {
        return -1;
    }
        

    const ProtectionZone *src = zone(m_active_id);
    if (!src)
    {
        return -1;
    }
    ProtectionZone copy = *src;
    copy.id = 0;
    copy.name += tr(" (copy)");
    copy.points.clear();
    for (const QPointF &p : src->points)
    {
        copy.points.append(p + QPointF(0.3, 0.3));
    }
    // 复制到同 Bank 的下一个空闲输出通道
    for (int out = 1; out <= kOutputCount; ++out) 
    {
        if (!zoneAt(src->bankIndex, out)) 
        {
            copy.outputIndex = out;
            break;
        }
    }

    int newId = addZone(copy);
    setActiveZoneId(newId);
    return newId;
}

bool ProtectionManager::eraseActiveZone()
{
    if (m_active_id < 0)
    {
        return false;
    }
    ProtectionZone *z = zone(m_active_id);
    if (!z)
    {
        return false;
    }
    z->points.clear();
    z->radius = 0.0;
    z->width = 0.0;
    z->height = 0.0;
    updateZone(z->id, *z);
    return true;
}

// ===========================================================================
// 持久化
// ===========================================================================
bool ProtectionManager::saveToFile(const QString &path) const
{
    QJsonArray arr;
    for (const ProtectionZone &z : m_zones)
    {
        arr.append(z.toJson());
    }
    QJsonObject root;
    root["version"]    = "2.0";
    root["bankCount"]  = kBankCount;
    root["outputCount"]= kOutputCount;
    root["activeZoneId"]= m_active_id;
    root["zones"]      = arr;

    QJsonDocument doc(root);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        return false;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    QFileInfo fileInfo(path);
    // 自动忽略路径，直接得到纯文件名 "device_setting"
    QString pure_name = fileInfo.baseName();
    pure_name+=".bin";
    BinManager::GetInstance().Save(pure_name,m_zones);
    return true;
}

bool ProtectionManager::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError)
    {
        return false;
    }
    QJsonObject root = doc.object();
    QJsonArray arr   = root["zones"].toArray();

    m_zones.clear();
    m_next_id = 1;
    for (const QJsonValue &v : arr) 
    {
        ProtectionZone z = ProtectionZone::zoneFromJson(v.toObject());
        // 旧版本 JSON 无 id 字段，分配新 id
        if (z.id <= 0)
            z.id = m_next_id;
        if (z.id >= m_next_id)
        {
            m_next_id = z.id + 1;
        }  
        m_zones.append(z);
    }

    m_active_id = root["activeZoneId"].toInt(-1);
    emit activeZoneChanged(m_active_id);
    emit zonesChanged();
    return true;
}
