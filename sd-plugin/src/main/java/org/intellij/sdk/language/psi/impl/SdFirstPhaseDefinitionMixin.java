package org.intellij.sdk.language.psi.impl;

import com.intellij.extapi.psi.ASTWrapperPsiElement;
import com.intellij.lang.ASTNode;
import com.intellij.navigation.ItemPresentation;
import com.intellij.psi.util.PsiTreeUtil;
import org.intellij.sdk.language.SdIcons;
import org.intellij.sdk.language.psi.SdRankProfileDefinition;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import javax.swing.Icon;

/**
 * This class is used for methods' implementations for SdFirstPhaseDefinition. Connected with "mixin" to 
 * FirstPhaseDefinition rule in sd.bnf
 * @author Shahar Ariel
 */
public class SdFirstPhaseDefinitionMixin extends ASTWrapperPsiElement {
    
    public SdFirstPhaseDefinitionMixin(@NotNull ASTNode node) {
        super(node);
    }
    
    @NotNull
    public String getName() {
        SdRankProfileDefinition rankProfile = PsiTreeUtil.getParentOfType(this, SdRankProfileDefinition.class);
        if (rankProfile == null) {
            return "";
        }
        return "first-phase of " + rankProfile.getName();
    }
    
    public ItemPresentation getPresentation() {
        final SdFirstPhaseDefinitionMixin element = this;
        return new ItemPresentation() {
            @Override
            public String getPresentableText() {
                SdRankProfileDefinition rankProfile = PsiTreeUtil.getParentOfType(element, SdRankProfileDefinition.class);
                if (rankProfile == null) {
                    return "";
                }
                return "first-phase of " + rankProfile.getName();
            }
            
            @Nullable
            @Override
            public String getLocationString() {
                return element.getContainingFile() != null ? element.getContainingFile().getName() : null;
            }
            
            @Override
            public Icon getIcon(boolean unused) {
                return SdIcons.FIRST_PHASE;
            }
        };
    }
}
