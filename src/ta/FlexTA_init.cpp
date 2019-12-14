/* Authors: Lutong Wang and Bangqi Xu */
/*
 * Copyright (c) 2019, The Regents of the University of California
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ta/FlexTA.h"

using namespace std;
using namespace fr;

void FlexTAWorker::initTracks() {
  //bool enableOutput = true;
  bool enableOutput = false;
  //tracks.clear();
  //tracks.resize(getDesign()->getTech()->getLayers().size());
  trackLocs.clear();
  trackLocs.resize(getDesign()->getTech()->getLayers().size());
  vector<set<frCoord> > trackCoordSets(getDesign()->getTech()->getLayers().size());
  // uPtr for tp
  for (int lNum = 0; lNum < (int)getDesign()->getTech()->getLayers().size(); lNum++) {
    auto layer = getDesign()->getTech()->getLayer(lNum);
    if (layer->getType() != frLayerTypeEnum::ROUTING) {
      continue;
    }
    if (layer->getDir() != getDir()) {
      continue;
    }
    for (auto &tp: getDesign()->getTopBlock()->getTrackPatterns(lNum)) {
      if ((getDir() == frcHorzPrefRoutingDir && tp->isHorizontal() == false) ||
          (getDir() == frcVertPrefRoutingDir && tp->isHorizontal() == true)) {
        if (enableOutput) {
          cout <<"TRACKS " <<(tp->isHorizontal() ? string("X ") : string("Y "))
               <<tp->getStartCoord() <<" DO " <<tp->getNumTracks() <<" STEP "
               <<tp->getTrackSpacing() <<" LAYER " <<tp->getLayerNum() 
               <<" ;" <<endl;
        }
        bool isH = (getDir() == frcHorzPrefRoutingDir);
        frCoord tempCoord1 = (isH ? getRouteBox().bottom() : getRouteBox().left());
        frCoord tempCoord2 = (isH ? getRouteBox().top()    : getRouteBox().right());
        int trackNum = (tempCoord1 - tp->getStartCoord()) / (int)tp->getTrackSpacing();
        if (trackNum < 0) {
          trackNum = 0;
        }
        if (trackNum * (int)tp->getTrackSpacing() + tp->getStartCoord() < tempCoord1) {
          trackNum++;
        }
        for (; trackNum < (int)tp->getNumTracks() && trackNum * (int)tp->getTrackSpacing() + tp->getStartCoord() < tempCoord2; trackNum++) {
          frCoord trackCoord = trackNum * tp->getTrackSpacing() + tp->getStartCoord();
          //cout <<"TRACKLOC " <<trackCoord * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<endl;
          trackCoordSets[lNum].insert(trackCoord);
        }
      }
    }
  }
  for (int i = 0; i < (int)trackCoordSets.size(); i++) {
    if (enableOutput) {
      cout <<"lNum " <<i <<":";
    }
    for (auto coord: trackCoordSets[i]) {
      if (enableOutput) {
        cout <<" " <<coord * 1.0 / getDesign()->getTopBlock()->getDBUPerUU();
      }
      //taTrack t;
      //t.setTrackLoc(coord);
      //tracks[i].push_back(std::move(t));
      trackLocs[i].push_back(coord);
    }
    if (enableOutput) {
      cout <<endl;
    }
  }
}

// use prefAp, otherwise return false
bool FlexTAWorker::initIroute_helper_pin(frGuide* guide, frCoord &maxBegin, frCoord &minEnd, 
                                         set<frCoord> &downViaCoordSet, set<frCoord> &upViaCoordSet,
                                         int &wlen, frCoord &wlen2) {
  bool enableOutput = false;
  frPoint bp, ep;
  guide->getPoints(bp, ep);
  if (!(bp == ep)) {
    return false;
  }

  auto net      = guide->getNet();
  auto layerNum = guide->getBeginLayerNum();
  bool isH      = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  bool hasDown  = false;
  bool hasUp    = false;

  vector<frGuide*> nbrGuides;
  auto rq = getRegionQuery();
  frBox box;
  box.set(bp, bp);
  nbrGuides.clear();
  if (layerNum - 2 >= 0) {
    rq->queryGuide(box, layerNum - 2, nbrGuides);
    for (auto &nbrGuide: nbrGuides) {
      if (nbrGuide->getNet() == net) {
        hasDown = true;
        break;
      }
    }
  } 
  nbrGuides.clear();
  if (layerNum + 2 < (int)design->getTech()->getLayers().size()) {
    rq->queryGuide(box, layerNum + 2, nbrGuides);
    for (auto &nbrGuide: nbrGuides) {
      if (nbrGuide->getNet() == net) {
        hasUp = true;
        break;
      }
    }
  } 

  vector<frBlockObject*> result;
  box.set(bp, bp);
  rq->queryGRPin(box, result);
  frTransform instXform; // (0,0), frcR0
  frTransform shiftXform;
  frTerm* trueTerm = nullptr;
  //string  name;
  for (auto &term: result) {
    bool hasInst = false;
    frInst* inst = nullptr;
    if (term->typeId() == frcInstTerm) {
      if (static_cast<frInstTerm*>(term)->getNet() != net) {
        continue;
      }
      hasInst = true;
      inst = static_cast<frInstTerm*>(term)->getInst();
      inst->getTransform(shiftXform);
      shiftXform.set(frOrient(frcR0));
      inst->getUpdatedXform(instXform);
      trueTerm = static_cast<frInstTerm*>(term)->getTerm();
    } else if (term->typeId() == frcTerm) {
      if (static_cast<frTerm*>(term)->getNet() != net) {
        continue;
      }
      trueTerm = static_cast<frTerm*>(term);
    }
    if (trueTerm) {
      int pinIdx = 0;
      int pinAccessIdx = (inst) ? inst->getPinAccessIdx() : -1;
      for (auto &pin: trueTerm->getPins()) {
        frAccessPoint* ap = nullptr;
        if (inst) {
          ap = (static_cast<frInstTerm*>(term)->getAccessPoints())[pinIdx];
        }
        if (!pin->hasPinAccess()) {
          continue;
        }
        if (pinAccessIdx == -1) {
          continue;
        }
        if (ap == nullptr) {
          continue;
        }
        frPoint apBp;
        ap->getPoint(apBp);
        if (enableOutput) {
          cout <<" (" <<apBp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                      <<apBp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") origin";
        }
        auto bNum = ap->getLayerNum();
        apBp.transform(shiftXform);
        if (layerNum == bNum && getRouteBox().contains(apBp)) {
          wlen2 = isH ? apBp.y() : apBp.x();
          maxBegin = isH ? apBp.x() : apBp.y();
          minEnd   = isH ? apBp.x() : apBp.y();
          wlen = 0;
          if (hasDown) {
            downViaCoordSet.insert(maxBegin);
          }
          if (hasUp) {
            upViaCoordSet.insert(maxBegin);
          }
          return true;
        }
        pinIdx++;
      }
    }
  }
  
  return false;
}

void FlexTAWorker::initIroute_helper(frGuide* guide, frCoord &maxBegin, frCoord &minEnd, 
                                     set<frCoord> &downViaCoordSet, set<frCoord> &upViaCoordSet,
                                     int &wlen, frCoord &wlen2) {
  if (!initIroute_helper_pin(guide, maxBegin, minEnd, downViaCoordSet, upViaCoordSet, wlen, wlen2)) {
    initIroute_helper_generic(guide, maxBegin, minEnd, downViaCoordSet, upViaCoordSet, wlen, wlen2);
  }
}


void FlexTAWorker::initIroute_helper_generic_helper(frGuide* guide, frCoord &wlen2) {
  bool enableOutput = false;

  frPoint bp, ep;
  guide->getPoints(bp, ep);
  auto net = guide->getNet();
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);

  auto rq = getRegionQuery();
  vector<frBlockObject*> result;

  frBox box;
  box.set(bp, bp);
  rq->queryGRPin(box, result);
  if (!(ep == bp)) {
    box.set(ep, ep);
    rq->queryGRPin(box, result);
  }
  frTransform instXform; // (0,0), frcR0
  frTransform shiftXform;
  frTerm* trueTerm = nullptr;
  //string  name;
  for (auto &term: result) {
    bool hasInst = false;
    frInst* inst = nullptr;
    if (term->typeId() == frcInstTerm) {
      if (static_cast<frInstTerm*>(term)->getNet() != net) {
        continue;
      }
      hasInst = true;
      inst = static_cast<frInstTerm*>(term)->getInst();
      inst->getTransform(shiftXform);
      shiftXform.set(frOrient(frcR0));
      inst->getUpdatedXform(instXform);
      trueTerm = static_cast<frInstTerm*>(term)->getTerm();
    } else if (term->typeId() == frcTerm) {
      if (static_cast<frTerm*>(term)->getNet() != net) {
        continue;
      }
      trueTerm = static_cast<frTerm*>(term);
    }
    if (trueTerm) {
      int pinIdx = 0;
      int pinAccessIdx = (inst) ? inst->getPinAccessIdx() : -1;
      for (auto &pin: trueTerm->getPins()) {
        frAccessPoint* ap = nullptr;
        if (inst) {
          ap = (static_cast<frInstTerm*>(term)->getAccessPoints())[pinIdx];
        }
        if (!pin->hasPinAccess()) {
          continue;
        }
        if (pinAccessIdx == -1) {
          continue;
        }
        if (ap == nullptr) {
          continue;
        }
        frPoint apBp;
        ap->getPoint(apBp);
        if (enableOutput) {
          cout <<" (" <<apBp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                      <<apBp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") origin";
        }
        apBp.transform(shiftXform);
        if (getRouteBox().contains(apBp)) {
          wlen2 = isH ? apBp.y() : apBp.x();
          return;
        }
        pinIdx++;
      }
    }
    ; // to do @@@@@
    wlen2 = 0;
  }
}

void FlexTAWorker::initIroute_helper_generic(frGuide* guide, frCoord &minBegin, frCoord &maxEnd, 
                                             set<frCoord> &downViaCoordSet, set<frCoord> &upViaCoordSet,
                                             int &wlen, frCoord &wlen2) {
  auto    net         = guide->getNet();
  auto    layerNum    = guide->getBeginLayerNum();
  bool    hasMinBegin = false;
  bool    hasMaxEnd   = false;
          minBegin    = std::numeric_limits<frCoord>::max();
          maxEnd      = std::numeric_limits<frCoord>::min();
          wlen        = 0;
          //wlen2       = std::numeric_limits<frCoord>::max();
  bool    isH         = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  downViaCoordSet.clear();
  upViaCoordSet.clear();
  frPoint nbrBp, nbrEp;
  frPoint nbrSegBegin, nbrSegEnd;
  
  frPoint bp, ep;
  guide->getPoints(bp, ep);
  frPoint cp;
  // layerNum in FlexTAWorker
  vector<frGuide*> nbrGuides;
  auto rq = getRegionQuery();
  frBox box;
  for (int i = 0; i < 2; i++) {
    nbrGuides.clear();
    // check left
    if (i == 0) {
      box.set(bp, bp);
      cp = bp;
    // check right
    } else {
      box.set(ep, ep);
      cp = ep;
    }
    if (layerNum - 2 >= 0) {
      rq->queryGuide(box, layerNum - 2, nbrGuides);
    } 
    if (layerNum + 2 < (int)design->getTech()->getLayers().size()) {
      rq->queryGuide(box, layerNum + 2, nbrGuides);
    } 
    for (auto &nbrGuide: nbrGuides) {
      if (nbrGuide->getNet() == net) {
        nbrGuide->getPoints(nbrBp, nbrEp);
        if (!nbrGuide->hasRoutes()) {
          // via location assumed in center
          auto psLNum = nbrGuide->getBeginLayerNum();
          if (psLNum == layerNum - 2) {
            downViaCoordSet.insert((isH ? nbrBp.x() : nbrBp.y()));
          } else {
            upViaCoordSet.insert((isH ? nbrBp.x() : nbrBp.y()));
          }
        } else {
          for (auto &connFig: nbrGuide->getRoutes()) {
            if (connFig->typeId() == frcPathSeg) {
              auto obj = static_cast<frPathSeg*>(connFig.get());
              obj->getPoints(nbrSegBegin, nbrSegEnd);
              auto psLNum = obj->getLayerNum();
              if (i == 0) {
                minBegin = min(minBegin, (isH ? nbrSegBegin.x() : nbrSegBegin.y()));
                hasMinBegin = true;
              } else {
                maxEnd = max(maxEnd, (isH ? nbrSegBegin.x() : nbrSegBegin.y()));
                hasMaxEnd = true;
              }
              //if (nbrBp == nbrEp) {
              //  wlen2 = isH ? ((nbrSegBegin.y() + nbrSegEnd().y()) / 2) : ((nbrSegBegin.x() + nbrSegEnd().x()) / 2)
              //}
              if (psLNum == layerNum - 2) {
                downViaCoordSet.insert((isH ? nbrSegBegin.x() : nbrSegBegin.y()));
              } else {
                upViaCoordSet.insert((isH ? nbrSegBegin.x() : nbrSegBegin.y()));
              }
            }
          }
        }
        if (cp == nbrEp) {
          wlen -= 1;
        }
        if (cp == nbrBp) {
          wlen += 1;
        }
      }
    }
  }

  if (!hasMinBegin) {
    minBegin = (isH ? bp.x() : bp.y());
  }
  if (!hasMaxEnd) {
    maxEnd = (isH ? ep.x() : ep.y());
  }
  if (minBegin > maxEnd) {
    swap(minBegin, maxEnd);
  }
  if (minBegin == maxEnd) {
    maxEnd += 1;
  }

  // wlen2 purely depends on ap regardless of track
  initIroute_helper_generic_helper(guide, wlen2);
}

void FlexTAWorker::initIroute(frGuide *guide) {
  bool enableOutput = false;
  //bool enableOutput = true;
  auto iroute = make_unique<taPin>();
  iroute->setGuide(guide);
  frBox guideBox;
  guide->getBBox(guideBox);
  auto layerNum = guide->getBeginLayerNum();
  bool isExt = !(getRouteBox().contains(guideBox));
  if (isExt) {
    // extIroute empty, skip
    if (guide->getRoutes().empty()) {
      return;
    }
    if (enableOutput) {
      double dbu = getDesign()->getTopBlock()->getDBUPerUU();
      cout <<"ext@(" <<guideBox.left() / dbu  <<", " <<guideBox.bottom() / dbu <<") ("
                     <<guideBox.right() / dbu <<", " <<guideBox.top()    / dbu <<")" <<endl;
    }
  }
  
  frCoord maxBegin, minEnd;
  set<frCoord> downViaCoordSet, upViaCoordSet;
  int wlen = 0;
  frCoord wlen2 = std::numeric_limits<frCoord>::max();
  initIroute_helper(guide, maxBegin, minEnd, downViaCoordSet, upViaCoordSet, wlen, wlen2);

  frCoord trackLoc = 0;
  frPoint segBegin, segEnd;
  bool    isH         = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  // set trackIdx
  if (!isInitTA()) {
    for (auto &connFig: guide->getRoutes()) {
      if (connFig->typeId() == frcPathSeg) {
        auto obj = static_cast<frPathSeg*>(connFig.get());
        obj->getPoints(segBegin, segEnd);
        trackLoc = (isH ? segBegin.y() : segBegin.x());
        //auto psLNum = obj->getLayerNum();
        //int idx1 = 0;
        //int idx2 = 0;
        //getTrackIdx(trackLoc, psLNum, idx1, idx2);
        //iroute.setTrackIdx(idx1);
      }
    }
  } else {
    trackLoc = 0;
  }

  unique_ptr<taPinFig> ps = make_unique<taPathSeg>();
  auto rptr = static_cast<taPathSeg*>(ps.get());
  if (isH) {
    rptr->setPoints(frPoint(maxBegin, trackLoc), frPoint(minEnd, trackLoc));
  } else {
    rptr->setPoints(frPoint(trackLoc, maxBegin), frPoint(trackLoc, minEnd));
  }
  rptr->setLayerNum(layerNum);
  rptr->setStyle(getDesign()->getTech()->getLayer(layerNum)->getDefaultSegStyle());
  // owner set when add to taPin
  iroute->addPinFig(ps);

  //iroute.setCoords(maxBegin, minEnd);
  //for (auto coord: upViaCoordSet) {
  //  iroute.addViaCoords(coord, true);
  //}
  //for (auto coord: downViaCoordSet) {
  //  iroute.addViaCoords(coord, false);
  //}
  for (auto coord: upViaCoordSet) {
    unique_ptr<taPinFig> via = make_unique<taVia>(getDesign()->getTech()->getLayer(layerNum + 1)->getDefaultViaDef());
    auto rViaPtr = static_cast<taVia*>(via.get());
    rViaPtr->setOrigin(isH ? frPoint(coord, trackLoc) : frPoint(trackLoc, coord));
    iroute->addPinFig(via);
  }
  for (auto coord: downViaCoordSet) {
    unique_ptr<taPinFig> via = make_unique<taVia>(getDesign()->getTech()->getLayer(layerNum - 1)->getDefaultViaDef());
    auto rViaPtr = static_cast<taVia*>(via.get());
    rViaPtr->setOrigin(isH ? frPoint(coord, trackLoc) : frPoint(trackLoc, coord));
    iroute->addPinFig(via);
  }
  iroute->setWlenHelper(wlen);
  if (wlen2 < std::numeric_limits<frCoord>::max()) {
    iroute->setWlenHelper2(wlen2);
  }
  addIroute(iroute, isExt);
  //iroute.setId(iroutes.size()); // set id
  //iroutes.push_back(iroute);
}



void FlexTAWorker::initIroutes() {
  //bool enableOutput = true;
  bool enableOutput = false;
  vector<rq_rptr_value_t<frGuide> > result;
  auto regionQuery = getRegionQuery();
  for (int lNum = 0; lNum < (int)getDesign()->getTech()->getLayers().size(); lNum++) {
    auto layer = getDesign()->getTech()->getLayer(lNum);
    if (layer->getType() != frLayerTypeEnum::ROUTING) {
      continue;
    }
    if (layer->getDir() != getDir()) {
      continue;
    }
    result.clear();
    regionQuery->queryGuide(getExtBox(), lNum, result);
    //cout <<endl <<"query1:" <<endl;
    for (auto &[boostb, guide]: result) {
      frPoint pt1, pt2;
      guide->getPoints(pt1, pt2);
      //if (enableOutput) {
      //  cout <<"found guide (" 
      //       <<pt1.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", " 
      //       <<pt1.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") (" 
      //       <<pt2.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", " 
      //       <<pt2.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") " 
      //       <<guide->getNet()->getName() << "\n";
      //}
      //guides.push_back(guide);
      //cout <<endl;
      initIroute(guide);
    }
    //sort(guides.begin(), guides.end(), [](const frGuide *a, const frGuide *b) {return *a < *b;});
  }

  if (enableOutput) {
    bool   isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    for (auto &iroute: iroutes) {
      frPoint bp, ep;
      frCoord bc, ec, trackLoc;
      cout <<iroute->getId() <<" " <<iroute->getGuide()->getNet()->getName();
      auto guideLNum = iroute->getGuide()->getBeginLayerNum();
      for (auto &uPinFig: iroute->getFigs()) {
        if (uPinFig->typeId() == tacPathSeg) {
          auto obj = static_cast<taPathSeg*>(uPinFig.get());
          obj->getPoints(bp, ep);
          bc = isH ? bp.x() : bp.y();
          ec = isH ? ep.x() : ep.y();
          trackLoc = isH ? bp.y() : bp.x();
          cout <<" (" <<bc / dbu <<"-->" <<ec / dbu <<"), len@" <<(ec - bc) / dbu <<", track@" <<trackLoc / dbu
               <<", " <<getDesign()->getTech()->getLayer(iroute->getGuide()->getBeginLayerNum())->getName();
        } else if (uPinFig->typeId() == tacVia) {
          auto obj = static_cast<taVia*>(uPinFig.get());
          auto cutLNum = obj->getViaDef()->getCutLayerNum();
          obj->getOrigin(bp);
          bc = isH ? bp.x() : bp.y();
          cout <<string((cutLNum > guideLNum) ? ", U@" : ", D@") <<bc / dbu;
        }
      }
      cout <<", wlen_h@" <<iroute->getWlenHelper() <<endl;
    }
  }
}



void FlexTAWorker::initCosts() {
  //bool enableOutput = true;
  bool enableOutput = false;
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frPoint bp, ep;
  frCoord bc, ec;
  // init cost
  if (isInitTA()) {
    for (auto &iroute: iroutes) {
      auto pitch = getDesign()->getTech()->getLayer(iroute->getGuide()->getBeginLayerNum())->getPitch();
      for (auto &uPinFig: iroute->getFigs()) {
        if (uPinFig->typeId() == tacPathSeg) {
          auto obj = static_cast<taPathSeg*>(uPinFig.get());
          obj->getPoints(bp, ep);
          bc = isH ? bp.x() : bp.y();
          ec = isH ? ep.x() : ep.y();
          iroute->setCost(ec - bc + iroute->hasWlenHelper2() * pitch * 1000);
        }
      }
    }
  } else {
    auto &workerRegionQuery = getWorkerRegionQuery();
    // update worker rq
    for (auto &iroute: iroutes) {
      for (auto &uPinFig: iroute->getFigs()) {
        workerRegionQuery.add(uPinFig.get());
        addCost(uPinFig.get());
      }
    }
    for (auto &iroute: extIroutes) {
      for (auto &uPinFig: iroute->getFigs()) {
        workerRegionQuery.add(uPinFig.get());
        addCost(uPinFig.get());
      }
    }
    // update iroute cost
    for (auto &iroute: iroutes) {
      frUInt4 drcCost = 0;
      frCoord trackLoc = std::numeric_limits<frCoord>::max();
      for (auto &uPinFig: iroute->getFigs()) {
        if (uPinFig->typeId() == tacPathSeg) {
          static_cast<taPathSeg*>(uPinFig.get())->getPoints(bp, ep);
          if (isH) {
            trackLoc = bp.y();
          } else {
            trackLoc = bp.x();
          }
          break;
        }
      }
      if (trackLoc == std::numeric_limits<frCoord>::max()) {
        cout <<"Error: FlexTAWorker::initCosts does not find trackLoc" <<endl;
        exit(1);
      }
      //auto tmpCost = assignIroute_getCost(iroute.get(), trackLoc, drcCost);
      assignIroute_getCost(iroute.get(), trackLoc, drcCost);
      iroute->setCost(drcCost);
      totCost += drcCost;
      //iroute->setDrcCost(drcCost);
      //totDrcCost += drcCost;
      if (enableOutput && !isInitTA()) {
        bool   isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
        double dbu = getDesign()->getTopBlock()->getDBUPerUU();
        frPoint bp, ep;
        frCoord bc, ec, trackLoc;
        cout <<iroute->getId() <<" " <<iroute->getGuide()->getNet()->getName();
        auto guideLNum = iroute->getGuide()->getBeginLayerNum();
        for (auto &uPinFig: iroute->getFigs()) {
          if (uPinFig->typeId() == tacPathSeg) {
            auto obj = static_cast<taPathSeg*>(uPinFig.get());
            obj->getPoints(bp, ep);
            bc = isH ? bp.x() : bp.y();
            ec = isH ? ep.x() : ep.y();
            trackLoc = isH ? bp.y() : bp.x();
            cout <<" (" <<bc / dbu <<"-->" <<ec / dbu <<"), len@" <<(ec - bc) / dbu <<", track@" <<trackLoc / dbu
                 <<", " <<getDesign()->getTech()->getLayer(iroute->getGuide()->getBeginLayerNum())->getName();
          } else if (uPinFig->typeId() == tacVia) {
            auto obj = static_cast<taVia*>(uPinFig.get());
            auto cutLNum = obj->getViaDef()->getCutLayerNum();
            obj->getOrigin(bp);
            bc = isH ? bp.x() : bp.y();
            cout <<string((cutLNum > guideLNum) ? ", U@" : ", D@") <<bc / dbu;
          }
        }
        //cout <<", wlen_h@" <<iroute->getWlenHelper() <<", cost@" <<iroute->getCost() <<", drcCost@" <<iroute->getDrcCost() <<endl;
        cout <<", wlen_h@" <<iroute->getWlenHelper() <<", cost@" <<iroute->getCost() <<endl;
      }
    }
  }
}

void FlexTAWorker::sortIroutes() {
  //bool enableOutput = true;
  bool enableOutput = false;
  // init cost
  if (isInitTA()) {
    for (auto &iroute: iroutes) {
      addToReassignIroutes(iroute.get());
    }
  } else {
    for (auto &iroute: iroutes) {
      if (iroute->getCost()) {
        addToReassignIroutes(iroute.get());
      }
    }
  }
  if (enableOutput && !isInitTA()) {
    bool   isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    for (auto &iroute: reassignIroutes) {
      frPoint bp, ep;
      frCoord bc, ec, trackLoc;
      cout <<iroute->getId() <<" " <<iroute->getGuide()->getNet()->getName();
      auto guideLNum = iroute->getGuide()->getBeginLayerNum();
      for (auto &uPinFig: iroute->getFigs()) {
        if (uPinFig->typeId() == tacPathSeg) {
          auto obj = static_cast<taPathSeg*>(uPinFig.get());
          obj->getPoints(bp, ep);
          bc = isH ? bp.x() : bp.y();
          ec = isH ? ep.x() : ep.y();
          trackLoc = isH ? bp.y() : bp.x();
          cout <<" (" <<bc / dbu <<"-->" <<ec / dbu <<"), len@" <<(ec - bc) / dbu <<", track@" <<trackLoc / dbu
               <<", " <<getDesign()->getTech()->getLayer(iroute->getGuide()->getBeginLayerNum())->getName();
        } else if (uPinFig->typeId() == tacVia) {
          auto obj = static_cast<taVia*>(uPinFig.get());
          auto cutLNum = obj->getViaDef()->getCutLayerNum();
          obj->getOrigin(bp);
          bc = isH ? bp.x() : bp.y();
          cout <<string((cutLNum > guideLNum) ? ", U@" : ", D@") <<bc / dbu;
        }
      }
      //cout <<", wlen_h@" <<iroute->getWlenHelper() <<", cost@" <<iroute->getCost() <<", drcCost@" <<iroute->getDrcCost() <<endl;
      cout <<", wlen_h@" <<iroute->getWlenHelper() <<", cost@" <<iroute->getCost() <<endl;
    }
  }
}

void FlexTAWorker::initFixedObjs_helper(const frBox &box, frCoord bloatDist, frLayerNum lNum, frNet* net) {
  //bool enableOutput = true;
  bool enableOutput = false;
  double dbu = getTech()->getDBUPerUU();
  frBox bloatBox;
  box.bloat(bloatDist, bloatBox);
  auto con = getDesign()->getTech()->getLayer(lNum)->getShortConstraint();
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  int idx1, idx2;
  //frCoord x1, x2;
  if (isH) {
    getTrackIdx(bloatBox.bottom(), bloatBox.top(),   lNum, idx1, idx2);
    //x1 = bloatBox.bottom();
    //x2 = bloatBox.top();
  } else {
    getTrackIdx(bloatBox.left(),   bloatBox.right(), lNum, idx1, idx2);
    //x1 = bloatBox.left();
    //x2 = bloatBox.right();
  }
  auto &trackLocs = getTrackLocs(lNum);
  //auto &tracks = getTracks(lNum);
  auto &workerRegionQuery = getWorkerRegionQuery();
  for (int i = idx1; i <= idx2; i++) {
    // new
    //auto &track = tracks[i];
    //track.addToCost(net, x1, x2, 0);
    //track.addToCost(net, x1, x2, 1);
    //track.addToCost(net, x1, x2, 2);
    // old
    auto trackLoc = trackLocs[i];
    frBox tmpBox;
    if (isH) {
      tmpBox.set(bloatBox.left(), trackLoc, bloatBox.right(), trackLoc);
    } else {
      tmpBox.set(trackLoc, bloatBox.bottom(), trackLoc, bloatBox.top());
    }
    workerRegionQuery.addCost(tmpBox, lNum, net, con);
    if (enableOutput) {
      cout <<"  add fixed obj cost ("
           <<tmpBox.left()  / dbu <<", " <<tmpBox.bottom() / dbu <<") (" 
           <<tmpBox.right() / dbu <<", " <<tmpBox.top()    / dbu <<") " 
           <<getDesign()->getTech()->getLayer(lNum)->getName();
      if (net != nullptr) {
        cout <<" " <<net->getName();
      }
      cout <<endl <<flush;
    }
  }
}


void FlexTAWorker::initFixedObjs() {
  //bool enableOutput = false;
  ////bool enableOutput = true;
  //double dbu = getTech()->getDBUPerUU();
  vector<rq_rptr_value_t<frBlockObject> > result;
  frBox box;
  frCoord width = 0;
  frCoord bloatDist = 0;
  for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
    result.clear();
    if (getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING ||
        getTech()->getLayer(layerNum)->getDir()  != getDir()) {
      continue;
    }
    width = getTech()->getLayer(layerNum)->getWidth();
    getRegionQuery()->query(getExtBox(), layerNum, result);
    for (auto &[boostb, obj]: result) {
      // instterm term
      if (obj->typeId() == frcTerm || obj->typeId() == frcInstTerm) {
        box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
        bloatDist = TASHAPEBLOATWIDTH * width;
        frNet* netPtr = nullptr;
        if (obj->typeId() == frcTerm) {
          netPtr = static_cast<frTerm*>(obj)->getNet();
        } else {
          netPtr = static_cast<frInstTerm*>(obj)->getNet();
        }
        initFixedObjs_helper(box, bloatDist, layerNum, netPtr);
      // snet
      } else if (obj->typeId() == frcPathSeg || obj->typeId() == frcVia) {
        box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
        bloatDist = TASHAPEBLOATWIDTH * width;
        frNet* netPtr = nullptr;
        if (obj->typeId() == frcPathSeg) {
          netPtr = static_cast<frPathSeg*>(obj)->getNet();
        } else {
          netPtr = static_cast<frVia*>(obj)->getNet();
        }
        initFixedObjs_helper(box, bloatDist, layerNum, netPtr);
      } else if (obj->typeId() == frcBlockage || obj->typeId() == frcInstBlockage) {
        if (USEMINSPACING_OBS) {
          box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
          bloatDist = TASHAPEBLOATWIDTH * width;
          initFixedObjs_helper(box, bloatDist, layerNum, nullptr);
        } else {
          box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
          bloatDist = TASHAPEBLOATWIDTH * width;
          initFixedObjs_helper(box, bloatDist, layerNum, nullptr);
        }
      } else {
        cout <<"Warning: unsupported type in initFixedObjs" <<endl;
      }
    }
  }
}


void FlexTAWorker::init() {
  rq.init();
  initTracks();
  if (getTAIter() != -1) {
    initFixedObjs();
  }
  initIroutes();
  if (getTAIter() != -1) {
    initCosts();
    sortIroutes();
  }
}