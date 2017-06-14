// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;
using CryEngine.Rendering;

namespace CryEngine
{
	/// <summary>
	/// Static class exposing access to the current view camera
	/// </summary>
	public static class Camera
	{
		/// <summary>
		/// Set or get the position of the current view camera.
		/// </summary>
		public static Vector3 Position
		{
			set
			{
				Engine.System.GetViewCamera().SetPosition(value);
			}
			get
			{
				return Engine.System.GetViewCamera().GetPosition();
			}
		}

		/// <summary>
		/// Get or set the facing direction of the current view camera.
		/// </summary>
		public static Vector3 ForwardDirection
		{
			set
			{
				var camera = Engine.System.GetViewCamera();
				var newRotation = new Quaternion(value);

				camera.SetMatrix(new Matrix3x4(Vector3.One, newRotation, camera.GetPosition()));
			}
			get
			{
				return Engine.System.GetViewCamera().GetMatrix().GetColumn1();
			}
		}

		/// <summary>
		/// Get or set the transformation matrix of the current view camera.
		/// </summary>
		public static Matrix3x4 Transform
		{
			set
			{
				Engine.System.GetViewCamera().SetMatrix(value);
			}
			get
			{
				return Engine.System.GetViewCamera().GetMatrix();
			}
		}

		/// <summary>
		/// Get or set the rotation of the current view camera
		/// </summary>
		public static Quaternion Rotation
		{
			set
			{
				var camera = Engine.System.GetViewCamera();

				camera.SetMatrix(new Matrix3x4(Vector3.One, value, camera.GetPosition()));
			}
			get
			{
				return new Quaternion(Engine.System.GetViewCamera().GetMatrix());
			}
		}

		/// <summary>
		/// Gets or sets the field of view by CVar.
		/// </summary>
		/// <value>The field of view.</value>
		public static float FieldOfView
		{
			get
			{
				return MathHelpers.RadiansToDegrees(Engine.System.GetViewCamera().GetFov());
			}
			set
			{
				var camera = Engine.System.GetViewCamera();

				camera.SetFrustum(Global.gEnv.pRenderer.GetWidth(), Global.gEnv.pRenderer.GetHeight(), MathHelpers.DegreesToRadians(value));
			}
		}

		/// <summary>
		/// Converts a screenpoint from screen-space to world-space.
		/// </summary>
		/// <param name="x"></param>
		/// <param name="y"></param>
		/// <returns></returns>
		public static Vector3 Unproject(int x, int y)
		{
			return Global.gEnv.pRenderer.UnprojectFromScreen(x, Renderer.ScreenHeight - y);
		}

		/// <summary>
		/// Converts a point in world-space to screen-space.
		/// </summary>
		/// <param name="position"></param>
		/// <returns></returns>
		public static Vector2 ProjectToScreen(Vector3 position)
		{
			return Global.gEnv.pRenderer.ProjectToScreen(position);
		}

		#region CCAmera-methods

		/// <summary>
		/// x-YAW
		/// y-PITCH (negative=looking down / positive=looking up)
		/// z-ROLL
		/// Note: If we are looking along the z-axis, its not possible to specify the x and z-angle.
		/// </summary>
		/// <param name="m"></param>
		/// <returns></returns>
		public static Angles3 CreateAnglesYPR(Matrix3x4 m)
		{
			Matrix33 m33 = new Matrix33(m);
			return CCamera.CreateAnglesYPR(m33);
		}

		/// <summary>
		/// This function builds a 3x3 orientation matrix using YPR-angles, and converts it to a Matrix3x4
		/// Rotation order for the orientation-matrix is Z-X-Y. (Zaxis=YAW / Xaxis=PITCH / Yaxis=ROLL)
		/// COORDINATE-SYSTEM
		/// z-axis
		///  ^
		///  |
		///  |  y-axis
		///  |  /
		///  | /
		///  |/
		///  +--------------->   x-axis
		/// </summary>
		/// <param name="ypr"></param>
		/// <returns></returns>
		public static Matrix3x4 CreateOrientationYPR(Angles3 ypr)
		{
			return new Matrix34(CCamera.CreateOrientationYPR(ypr));
		}

		/// <summary>
		/// x=yaw
		/// y=pitch
		/// z=roll (we ignore this element, since its not possible to convert the roll-component into a vector)
		/// </summary>
		/// <param name="ypr"></param>
		/// <returns></returns>
		public static Vector3 CreateViewdir(Angles3 ypr)
		{
			return CCamera.CreateViewdir(ypr);
		}

		#endregion
	}
}
