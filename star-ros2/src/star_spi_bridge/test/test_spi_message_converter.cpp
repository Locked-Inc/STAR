#include <gtest/gtest.h>
#include "star_spi_bridge/spi_message_converter.hpp"

using namespace star_spi_bridge;

class SpiMessageConverterTest : public ::testing::Test {
protected:
    SpiMessageConverter::Parameters params{0.150, 0.0325, 11599};
    SpiMessageConverter converter{params};
};

TEST_F(SpiMessageConverterTest, TwistToVelocity_Forward) {
    geometry_msgs::msg::Twist twist;
    twist.linear.x = 1.0;
    twist.angular.z = 0.0;
    
    star::v1::VelocityCommand cmd;
    EXPECT_TRUE(converter.twist_to_velocity_command(twist, cmd));
    
    EXPECT_NEAR(cmd.front_left_velocity_mps(), 1.0, 0.001);
    EXPECT_NEAR(cmd.front_right_velocity_mps(), 1.0, 0.001);
}

TEST_F(SpiMessageConverterTest, TwistToVelocity_RotateLeft) {
    geometry_msgs::msg::Twist twist;
    twist.linear.x = 0.0;
    twist.angular.z = 2.0; // 2 rad/s
    
    star::v1::VelocityCommand cmd;
    EXPECT_TRUE(converter.twist_to_velocity_command(twist, cmd));
    
    // v_right = 0 + 2 * (0.15/2) = 0.15
    // v_left = 0 - 2 * (0.15/2) = -0.15
    EXPECT_NEAR(cmd.front_right_velocity_mps(), 0.15, 0.001);
    EXPECT_NEAR(cmd.front_left_velocity_mps(), -0.15, 0.001);
}

TEST_F(SpiMessageConverterTest, TwistToVelocity_NaN) {
    geometry_msgs::msg::Twist twist;
    twist.linear.x = std::numeric_limits<double>::quiet_NaN();
    
    star::v1::VelocityCommand cmd;
    EXPECT_FALSE(converter.twist_to_velocity_command(twist, cmd));
}
