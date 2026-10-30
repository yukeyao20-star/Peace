// 降水粒子着色器（雨丝 / 雪花）——带"屋檐遮蔽"，让雨雪不落进屋里
//
// 为什么不用 StandardMaterial3D ——
//   雨雪粒子挂在 RainRoot 下，RainRoot 每帧跟着主角走，发射盒覆盖主角周围
//   约 26×26m。木屋就在盒子里，所以粒子会直接穿屋顶落进屋里。
//   要修就得在顶点阶段按世界坐标把"屋檐下"的粒子剔掉，而 StandardMaterial3D
//   没法插自定义顶点逻辑。
//
//   换成自定义 shader 的代价：Godot 4 的 shader **没有 billboard render_mode**
//   （实测 `render_mode billboard` / `billboard_keep_scale` / `fixed_y_billboard`
//   全部报 `Invalid render mode`；billboard 只是 StandardMaterial3D 的属性，
//   由它内部生成的 shader 实现）。所以这里手写 billboard。
//
// 两个实测确认的事实（别再猜）：
//   1. CPUParticles3D 的顶点着色器里，`MODEL_MATRIX` 已经是**世界坐标**
//      （含父节点 RainRoot 的变换）。验证脚本 project/tests/qa_modelmatrix.gd：
//      把发射器挂到世界 x=1000 的父节点下，用 step(500, MODEL_MATRIX[3].x)
//      判定，屏幕中心像素是红的 → 世界坐标。
//   2. 粒子自身的缩放（PARAM_SCALE）藏在 MODEL_MATRIX 的基里，取基向量长度即可。
//
// 遮蔽体为什么用"木屋局部空间"而不是世界 AABB ——
//   木屋整体绕 Y 转了 45°。世界 AABB 会是外接正方形，比木屋大一圈，
//   结果就是屋子周围凭空出现一块菱形"无雨区"。所以这里把粒子的世界位置
//   绕 Y 轴反转到木屋局部坐标，再在局部空间里做盒体判定。
//   木屋是纯 Y 轴旋转，所以只需要 cos/sin 两个 float，不用 mat4
//   （也就不用赌 Godot 对 mat4 uniform 的类型转换）。
//
// 遮蔽体形状 = 局部盒体 ∩ (y < 屋面高度)。
//   屋面高度默认水平；sslope = (slope_x, x0, slope_z, z0) 给出沿局部 X / Z 的
//   下斜率与基准点，gable 屋顶两侧对称 → 取 |x - x0| / |z - z0|。
//   命中的粒子把四边形**塌成一个点**：零面积三角形在光栅化阶段直接丢弃，
//   连片元都不会产生（用 discard 反而更贵，且透明排序仍会处理它）。

shader_type spatial;
// blend_mix：写 ALPHA 走透明混合（默认值，写出来只是为了让"透明"这件事显式可见）
// depth_draw_never：雨雪不写深度，与旧版 StandardMaterial3D(TRANSPARENCY_ALPHA)
//   的行为一致（透明材质默认不写深度），避免雨丝互相遮挡出硬边
render_mode unshaded, blend_mix, cull_disabled, depth_draw_never, skip_vertex_transform;

uniform vec4 tint : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform bool fixed_y = true; // true = 只绕世界 Y 轴朝向相机（雨丝）；false = 全向（雪花）

// —— 遮蔽体：全部在木屋局部空间描述 ——
uniform int shelter_count = 0;
uniform vec3 shelter_origin = vec3(0.0); // 木屋世界原点
uniform float shelter_cos = 1.0;
uniform float shelter_sin = 0.0;

// 0 号：主屋坡屋顶（最大、最要紧）
uniform vec3 smin0 = vec3(0.0);
uniform vec3 smax0 = vec3(0.0);
uniform vec4 sslope0 = vec4(0.0);
// 1 号：东侧柴棚
uniform vec3 smin1 = vec3(0.0);
uniform vec3 smax1 = vec3(0.0);
uniform vec4 sslope1 = vec4(0.0);
// 2 号：南侧门廊雨棚
uniform vec3 smin2 = vec3(0.0);
uniform vec3 smax2 = vec3(0.0);
uniform vec4 sslope2 = vec4(0.0);

varying vec4 vcol;

// 局部点 lp 是否落在 [lo,hi] 盒体里、且低于屋面高度
bool under_roof(vec3 lp, vec3 lo, vec3 hi, vec4 sl) {
	if (lp.x <= lo.x || lp.x >= hi.x || lp.z <= lo.z || lp.z >= hi.z || lp.y <= lo.y) {
		return false;
	}
	float top = hi.y - sl.x * abs(lp.x - sl.y) - sl.z * abs(lp.z - sl.w);
	return lp.y < top;
}

void vertex() {
	vcol = COLOR;

	// 粒子世界位置（见文件头事实 1）
	vec3 origin = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	// —— 屋檐遮蔽：落到屋里/屋檐下的粒子标记为隐藏 ——
	bool hidden = false;
	if (shelter_count > 0) {
		vec3 rel = origin - shelter_origin;
		// 世界 → 木屋局部（绕 Y 反转 yaw）
		vec3 lp = vec3(rel.x * shelter_cos - rel.z * shelter_sin,
				rel.y,
				rel.x * shelter_sin + rel.z * shelter_cos);
		hidden = under_roof(lp, smin0, smax0, sslope0);
		if (!hidden && shelter_count > 1) {
			hidden = under_roof(lp, smin1, smax1, sslope1);
		}
		if (!hidden && shelter_count > 2) {
			hidden = under_roof(lp, smin2, smax2, sslope2);
		}
	}

	// —— 手写 billboard ——
	vec3 to_cam = INV_VIEW_MATRIX[3].xyz - origin;
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right;
	if (fixed_y) {
		// 只绕世界 Y 轴转向相机：雨丝始终保持竖直，俯视时不会变成横杠
		vec2 flat_dir = vec2(to_cam.x, to_cam.z);
		if (dot(flat_dir, flat_dir) < 1e-6) {
			flat_dir = vec2(0.0, 1.0);
		}
		vec3 f = normalize(vec3(flat_dir.x, 0.0, flat_dir.y));
		right = normalize(cross(up, f));
	} else {
		// 全向朝向相机：雪花在各个角度看都是同一片
		vec3 f = normalize(to_cam);
		vec3 r = cross(up, f);
		right = (length(r) < 1e-4) ? vec3(1.0, 0.0, 0.0) : normalize(r);
		up = normalize(cross(f, right));
	}

	float s = length(MODEL_MATRIX[0].xyz); // 粒子缩放
	vec3 wp = hidden ? origin : origin + (right * VERTEX.x + up * VERTEX.y) * s;

	// skip_vertex_transform：VERTEX / NORMAL 都要自己变到视图空间
	VERTEX = (VIEW_MATRIX * vec4(wp, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(0.0, 0.0, 1.0, 0.0)).xyz);
}

void fragment() {
	vec4 c = vcol * tint;
	ALBEDO = c.rgb;
	ALPHA = c.a;
	// —— 雪花形状（2026-10-31 用户反馈"优化雪花形状 ❄️"）——
	// 原来是一片均匀的方块；现在用四边形 UV 做程序化雪花：
	//   柔边圆盘（主体） + 六角星芒（外圈慢慢散开），近看是 ❄ 远看是柔和雪点。
	// 雨丝（fixed_y=true）保持原样，不做这个遮罩。
	if (!fixed_y) {
		vec2 p = UV - vec2(0.5);
		float d = length(p) * 2.0;
		float disc = smoothstep(1.0, 0.30, d);        // 中心柔边圆
		float ang = atan(p.y, p.x);
		// cos(6θ) 的峰方向每 60° 一根 → 六臂；只在外圈出现，中心保持圆点
		float arms = pow(max(cos(ang * 6.0), 0.0), 10.0);
		float ring = smoothstep(0.15, 0.55, d) * smoothstep(1.05, 0.55, d);
		ALPHA *= clamp(disc * 0.9 + arms * ring * 0.85, 0.0, 1.0);
	}
}
