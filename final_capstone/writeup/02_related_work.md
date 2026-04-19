# Related work

STAR sits at the intersection of three established research streams:
crowd-and-AI accessibility mapping, LiDAR-based ADA assessment, and
multi-sensor SLAM fusion.

The most influential body of accessibility-mapping work is **Jon Froehlich's
Makeability Lab at the University of Washington**. Saha et al., *Project
Sidewalk: A Web-based Crowdsourcing Tool for Collecting Sidewalk Accessibility
Data at Scale* (CHI 2019, Best Paper), demonstrated that a hybrid crowd+AI
pipeline can label sidewalk barriers across entire cities from Google Street
View imagery. Weld et al., *Deep Learning for Automatically Detecting Sidewalk
Accessibility Problems Using Streetscape Imagery* (ASSETS 2019, Best Student
Paper), automated the labeling step using CNN classifiers. Earlier work - Hara
et al.'s *Tohme: Detecting Curb Ramps in Google Street View Using Crowdsourcing,
Computer Vision, and Machine Learning* (UIST 2014) - established the curb-ramp
detection task. Most recently, Hosseini et al., *Towards Global-Scale Crowd+AI
Techniques to Map and Assess Sidewalks for People with Disabilities* (CVPR 2022
AVA Workshop, arXiv:2206.13677), framed the problem at planetary scale. STAR
adopts the social-impact framing of Project Sidewalk but moves the sensing
modality from passive street-view imagery to active onboard 3D measurement, and
the environment from outdoor sidewalks to indoor buildings.

For **LiDAR-based ADA assessment specifically**, the closest precedent is the
Oregon State University work led by Yelda Turkan with Erzhuo Che, *Automated
Localization and ADA Functional Condition Assessment of Curb Ramps using Mobile
Lidar*, which uses vehicle-mounted mobile LiDAR to identify and assess curb
ramps at intersection scale. **CloudPoint Geospatial's** 2022 industry white
paper *Using GIS & LiDAR to Conduct an ADA Site Assessment* documents the
manual workflow that LiDAR-equipped human surveyors use today. **ROCK Robotic**
has publicly marketed its R3 Pro V2 handheld LiDAR (5 mm range accuracy, 1.28M
points/sec) for "ADA Compliance at Scale" (LinkedIn, September 2024) - a sensor
a human carries, not an autonomous platform. None of these efforts close the
loop from autonomous navigation through compliance reporting in a single system.

For **multi-sensor SLAM fusion**, recent peer-reviewed work supports STAR's
stereo+LiDAR+IMU architecture. *Stereo and LiDAR Loosely Coupled SLAM
Constrained by Ground Detection* (Sensors 2024, PMC11548508) demonstrates that
ground-plane coplanarity constraints improve fused pose accuracy in indoor
environments. Lang et al., *Gaussian-LIC: Real-Time Photo-Realistic SLAM with
Gaussian Splatting and LiDAR-Inertial-Camera Fusion* (arXiv:2404.06926, 2024),
and the *LVI-Fusion* tightly-coupled scheme published in MDPI Remote Sensing
16(9):1524 (2024), both report that fused stereo+LiDAR+IMU outperforms any
single modality on indoor pose accuracy and dense reconstruction - the
empirical claim STAR's validation methodology is designed to reproduce.
