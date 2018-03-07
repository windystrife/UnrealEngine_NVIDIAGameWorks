from Modules.p4python.P4 import P4, P4Exception
import maya.cmds as cmds

def findFileP4(asset, debug=0, root='//depot/ArtSource/'):
    p4 = P4()
    try: p4.connect()
    except:
        print 'Cannot connect!'
        return False

    try:
        file =  p4.run_files(root + '...' + asset)
        depotLoc = file[0]['depotFile']
        describe = p4.run_describe(file[0]['change'])
        return [describe[0]['user'], depotLoc, describe[0]['desc'], file[0]['change']]
    except Exception as e:
        print "Cannot find file.", asset
        if debug: print e
        return False
    finally:
        p4.disconnect()

def rePathFileNodesP4(debug=0, searchPath='//depot/ArtSource/'):
    nodes = cmds.ls(type='file')

    for node in nodes:
        path = cmds.getAttr((node + '.fileTextureName'))
        newPath = None

        #check if the file exists on disk
        if cmds.file(path, exists=1, q=1):
            pass

        #if the file is not on disk, continue and get it from P4
        else:
            if debug:
                print 'rePathFileNodesP4>>>> Local file not found', path, 'searching p4 database.'

            #connect to the perforce server
            p4 = P4()
            try: p4.connect()
            except:
                print '>>rePathFileNodesP4: Cannot connect to perforce!'
                return False

            try:
                #convert the path to the same direction slashes
                floppedPath = path.replace('\\', '/')
                fName = floppedPath.split('/')[-1]

                #search p4 for the file
                f = None
                try:
                    f = p4.run_files(searchPath + '...' + fName)
                except:
                    pass
                if f:
                    if len(f) > 1:
                        print 'rePathFileNodesP4>>>> Multiple files [', len(f), '] found in depot search path with the name', f
                        for i in range(0, len(f)):
                            try:
                                print 'rePathFileNodesP4>>>> SYNCING: ', f[i]['depotFile']
                                p4.run_sync(f[i]['depotFile'])
                            except Exception as e:
                                if debug:
                                    print e
                    else:
                        print 'rePathFileNodesP4>>>> File found on depot: ', f[0]['depotFile']
                        try:
                            p4.run_sync(f[0]['depotFile'])
                        except Exception as e:
                                if debug:
                                    print e
                else:
                    print 'rePathFileNodesP4>>>> FILE NOT FOUND IN PERFORCE SEARCH PATH - fName:', fName, 'searchPath:', searchPath

                fileFstat = p4.run('fstat', f[0]['depotFile'])[0]
                newPath = fileFstat['clientFile']

                if debug: print 'rePathFileNodesP4>>>> NEW PATH>>', newPath, '\n\n'
                cmds.setAttr((node + '.fileTextureName'), newPath, type='string')
            except Exception as e:
                print "Cannot find file.", fName,e
                if debug:
                    print 'rePathFileNodesP4>>>> EXCEPTION', e
    p4.disconnect()