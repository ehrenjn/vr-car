/*
Copyright (c) 2010-2016, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MAPBUILDER_H_
#define MAPBUILDER_H_

#include <QVBoxLayout>
#include <QtCore/QMetaType>
#include <QAction>

#ifndef Q_MOC_RUN // Mac OS X issue
#include "rtabmap/gui/CloudViewer.h"
#include "rtabmap/core/util3d.h"
#include "rtabmap/core/util3d_filtering.h"
#include "rtabmap/core/util3d_transforms.h"
#include "rtabmap/core/OdometryInfo.h"
#include "rtabmap/core/Statistics.h"
#include "rtabmap/core/Signature.h"
#endif
#include "rtabmap/utilite/UStl.h"
#include "rtabmap/utilite/UConversion.h"
#include "rtabmap/utilite/ULogger.h"

#include "communicator/TCPCommunicator.h"
#include <stdio.h>
#define PORT 7787

using namespace rtabmap;

// This class receives RtabmapEvent and construct/update a 3D Map
class MapBuilder : public QWidget
{
	Q_OBJECT
public:
	//Camera ownership is not transferred!
	MapBuilder() :
		odometryCorrection_(Transform::getIdentity()),
		paused_(false),
		communicator(PORT),
		message(Message::createEmptyMessage(backingArray))
	{
		this->setWindowFlags(Qt::Dialog);
		this->setWindowTitle(tr("3D Map"));
		this->setMinimumWidth(800);
		this->setMinimumHeight(600);

		cloudViewer_ = new CloudViewer(this);

		QVBoxLayout *layout = new QVBoxLayout();
		layout->addWidget(cloudViewer_);
		this->setLayout(layout);

		QAction * pause = new QAction(this);
		this->addAction(pause);
		pause->setShortcut(Qt::Key_Space);
		connect(pause, SIGNAL(triggered()), this, SLOT(pauseDetection()));

		std::cout << "STARTED SERVER ON PORT " << PORT << std::endl;
		std::cout << "WAITING FOR CLIENT..." << std::endl;
		communicator.connectToClient();
		std::cout << "CLIENT CONNECTED" << std::endl;
	}

	virtual ~MapBuilder()
	{
	}

	bool isPaused() const {return paused_;}

	void processOdometry(
			const SensorData & data,
			Transform pose,
			const rtabmap::OdometryInfo & odom)
	{
		if(!this->isVisible())
		{
			return;
		}

		if(pose.isNull())
		{
			//Odometry lost
			cloudViewer_->setBackgroundColor(Qt::darkRed);

			pose = lastOdomPose_;
		}
		else
		{
			cloudViewer_->setBackgroundColor(cloudViewer_->getDefaultBackgroundColor());
		}
		if(!pose.isNull())
		{
			lastOdomPose_ = pose;

			// 3d cloud
			if(data.depthOrRightRaw().cols == data.imageRaw().cols &&
			   data.depthOrRightRaw().rows == data.imageRaw().rows &&
			   !data.depthOrRightRaw().empty() &&
			   (data.stereoCameraModel().isValidForProjection() || data.cameraModels().size()))
			{
				pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud = util3d::cloudRGBFromSensorData(
					data,
					4,     // decimation
					0.0f); // max depth
				if(cloud->size())
				{
					sendCloud(cloud, odometryCorrection_*pose);
					if(!cloudViewer_->addCloud("cloudOdom", cloud, odometryCorrection_*pose))
					{
						UERROR("Adding cloudOdom to viewer failed!");
					}
				}
				else
				{
					cloudViewer_->setCloudVisibility("cloudOdom", false);
					UWARN("Empty cloudOdom!");
				}
			}

			if(!pose.isNull())
			{
				// update camera position
				cloudViewer_->updateCameraTargetPosition(odometryCorrection_*pose);
			}
		}
		cloudViewer_->update();
	}


	void processStatistics(const rtabmap::Statistics & stats)
	{

		//============================
		// Add RGB-D clouds
		//============================
		const std::map<int, Transform> & poses = stats.poses();
		QMap<std::string, Transform> clouds = cloudViewer_->getAddedClouds();
		for(std::map<int, Transform>::const_iterator iter = poses.lower_bound(1); iter!=poses.end(); ++iter)
		{
			if(!iter->second.isNull())
			{
				std::string cloudName = uFormat("cloud%d", iter->first);

				// 3d point cloud
				if(clouds.contains(cloudName))
				{
					// Update only if the pose has changed
					Transform tCloud;
					cloudViewer_->getPose(cloudName, tCloud);
					if(tCloud.isNull() || iter->second != tCloud)
					{
						if(!cloudViewer_->updateCloudPose(cloudName, iter->second))
						{
							UERROR("Updating pose cloud %d failed!", iter->first);
						}
					}
					cloudViewer_->setCloudVisibility(cloudName, true);
				}
				else if(iter->first == stats.getLastSignatureData().id())
				{
					Signature s = stats.getLastSignatureData();
					s.sensorData().uncompressData(); // make sure data is uncompressed
					// Add the new cloud
					pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud = util3d::cloudRGBFromSensorData(
							s.sensorData(),
							4,     // decimation
							4.0f); // max depth
					if(cloud->size())
					{
						if(!cloudViewer_->addCloud(cloudName, cloud, iter->second))
						{
							UERROR("Adding cloud %d to viewer failed!", iter->first);
						}
					}
					else
					{
						UWARN("Empty cloud %d!", iter->first);
					}
				}
			}
			else
			{
				UWARN("Null pose for %d ?!?", iter->first);
			}
		}

		//============================
		// Add 3D graph (show all poses)
		//============================
		cloudViewer_->removeAllGraphs();
		cloudViewer_->removeCloud("graph_nodes");
		if(poses.size())
		{
			// Set graph
			pcl::PointCloud<pcl::PointXYZ>::Ptr graph(new pcl::PointCloud<pcl::PointXYZ>);
			pcl::PointCloud<pcl::PointXYZ>::Ptr graphNodes(new pcl::PointCloud<pcl::PointXYZ>);
			for(std::map<int, Transform>::const_iterator iter=poses.lower_bound(1); iter!=poses.end(); ++iter)
			{
				graph->push_back(pcl::PointXYZ(iter->second.x(), iter->second.y(), iter->second.z()));
			}
			*graphNodes = *graph;


			// add graph
			cloudViewer_->addOrUpdateGraph("graph", graph, Qt::gray);
			cloudViewer_->addCloud("graph_nodes", graphNodes, Transform::getIdentity(), Qt::green);
			cloudViewer_->setCloudPointSize("graph_nodes", 5);
		}

		odometryCorrection_ = stats.mapCorrection();

		cloudViewer_->update();
	}


	void sendCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, const Transform& pose) 
	{
		float* dataPtr = reinterpret_cast<float*>(message.getStartOfData());

		// add origin to data
		*dataPtr = pose.x(); dataPtr++;
		*dataPtr = pose.y(); dataPtr++;
		*dataPtr = pose.z(); dataPtr++;

		// add orientation to data
		Eigen::Quaternionf orientation = Eigen::Quaternionf(pose.toEigen3f().linear());
		*dataPtr = orientation.x(); dataPtr++;
		*dataPtr = orientation.y(); dataPtr++;
		*dataPtr = orientation.z(); dataPtr++;
		*dataPtr = orientation.w(); dataPtr++;

		// add points to data
		int numPointsToSend = 0;
		pcl::PointCloud<pcl::PointXYZRGB>::const_iterator point;
		for (point = cloud->begin(); point < cloud->end(); point++) {
			if (!std::isnan(point->x) && !std::isnan(point->y) && !std::isnan(point->z)) // only send points that are non NaN
			{
				dataPtr[0] = point->x;
				dataPtr[1] = point->y;
				dataPtr[2] = point->z;
				dataPtr[3] = point->rgb;
				dataPtr += 4;
				numPointsToSend ++;
			}
		}

		// update Message metadata
		int poseSize = sizeof(float) * (3 + 4); // origin + orientation 
		int cloudSize = numPointsToSend * sizeof(float) * 4; 
		message.setMetaData('c', cloudSize + poseSize);
		
		communicator.sendMessage(message);
	}

protected Q_SLOTS:
	void pauseDetection()
	{
		paused_ = !paused_;
	}

protected:
	CloudViewer * cloudViewer_;
	Transform lastOdomPose_;
	Transform odometryCorrection_;
	bool paused_;

private:
	TCPServer communicator;
	uint8_t backingArray[1000000];
	Message message;
};


#endif /* MAPBUILDER_H_ */
