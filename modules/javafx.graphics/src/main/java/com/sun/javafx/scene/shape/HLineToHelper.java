/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package com.sun.javafx.scene.shape;

import com.sun.javafx.geom.Path2D;
import com.sun.javafx.util.Utils;
import javafx.scene.shape.HLineTo;
import javafx.scene.shape.PathElement;

/**
 * Used to access internal methods of HLineTo.
 */
public class HLineToHelper extends PathElementHelper {

    private static final HLineToHelper theInstance;
    private static HLineToAccessor hLineToAccessor;

    static {
        theInstance = new HLineToHelper();
        Utils.forceInit(HLineTo.class);
    }

    private static HLineToHelper getInstance() {
        return theInstance;
    }

    public static void initHelper(HLineTo hLineTo) {
        setHelper(hLineTo, getInstance());
    }

    @Override
    protected  void addToImpl(PathElement pathElement, Path2D path) {
        hLineToAccessor.doAddTo(pathElement, path);
    }

    public static void setHLineToAccessor(final HLineToAccessor newAccessor) {
        if (hLineToAccessor != null) {
            throw new IllegalStateException();
        }

        hLineToAccessor = newAccessor;
    }

    public interface HLineToAccessor {
        void doAddTo(PathElement pathElement, Path2D path);
    }

}

