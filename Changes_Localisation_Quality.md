# Localisation Quality Changes

OpenXR currently provides localisation information with binary flags `POSITION_TRACKED / POSITION_VALID`, this is what Godot core uses in `XRPose.tracking_confidence`.
More continuous metrics were left to vendors to implement.
MagicLeap2 implements `XR_ML_localization_map_confidence` which gives 4 levels of tracking quality (0-3), so we have added this functionality to the add-on.
