package com.genymobile.scrcpy.util;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import java.util.ArrayList;
import java.util.List;

public class AppListProcessor {
    private final List<ResolveInfo> exactMatchesLabel = new ArrayList<>();
    private final List<ResolveInfo> potentialMatchesAppName = new ArrayList<>();
    private final List<ResolveInfo> potentialMatchesPkgName = new ArrayList<>();
    private final boolean isPackageName; //Treated as label if false
    private final String errorMessage;
    private final String query;

    public AppListProcessor(boolean isPackageName, boolean appDrawer, String orgQuery) {
        this.isPackageName = isPackageName;
        this.query = orgQuery;

        String tmpErrMsg = isPackageName ? "No launchable app with package name " : "No unique launchable app named ";
        this.errorMessage = tmpErrMsg + "\"" + orgQuery + "\" found from " + ( appDrawer ? "app drawer" : "list of all apps");
    }

    public String getOrgQuery(){
        return query;
    }

    public void addExactMatchesLabel(ResolveInfo resolveInfo) {
        exactMatchesLabel.add(resolveInfo);
    }

    public void addPotentialMatchesAppName(ResolveInfo resolveInfo) {
        potentialMatchesAppName.add(resolveInfo);
    }

    public void addPotentialMatchesPkgName(ResolveInfo resolveInfo) {
        potentialMatchesPkgName.add(resolveInfo);
    }

    public Intent getIntent(PackageManager pm, boolean showSuggestions){
        String suggestions = "\n";
        boolean multipleExactLabelMatches = false;
        if (isPackageName){
            if (!potentialMatchesPkgName.isEmpty()){
                suggestions+=LogUtils.buildAppListMessage("Found "+potentialMatchesPkgName.size()+" potential matches:",potentialMatchesPkgName);
            }
        } else {
            if (exactMatchesLabel.size() == 1){
                ActivityInfo activityInfo = exactMatchesLabel.get(0).activityInfo;
                ComponentName componentName = new ComponentName(activityInfo.packageName, activityInfo.name);
                return new Intent().setComponent(componentName)
                        .putExtra("APP_LABEL", exactMatchesLabel.get(0).loadLabel(pm).toString());
            } else{
                if (!exactMatchesLabel.isEmpty()){
                    multipleExactLabelMatches = true;
                    showSuggestions = true;
                    suggestions+=LogUtils.buildAppListMessage("Found "+exactMatchesLabel.size()+" exact matches:",exactMatchesLabel)+"\n";
                }
                if (!potentialMatchesAppName.isEmpty()){
                    suggestions+=LogUtils.buildAppListMessage("Found " + potentialMatchesAppName.size() + " other potential " + (potentialMatchesAppName.size() == 1 ? "match:" : "matches:"), potentialMatchesAppName)+"\n";
                }
            }
        }

        if (showSuggestions){
            Ln.e(errorMessage + (suggestions.equals("\n")? "\0" : suggestions));
        } else {
            Ln.e(errorMessage);
        }
        if (multipleExactLabelMatches){
            return new Intent().putExtra("MULTIPLE_EXACT_LABELS", true);
        } else {
            return null;
        }
    }
}
